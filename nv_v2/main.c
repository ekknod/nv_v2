#include <windows.h>
#include <math.h>

#pragma comment(lib, "ntdll.lib")

/* global variables */

static int    _target, _target_hitbox;
static int    _mp_teammates_are_enemies;
static int    _volume;
static float  _start_volume, _min_volume;

/* static hitboxes */

static int hitbox_list[6] = { 5, 4, 3, 0, 7, 8 } ;

/* functions */

HANDLE (WINAPI * _CreateThread)(PVOID, SIZE_T, PVOID, LPVOID, DWORD, LPDWORD);
BOOL (WINAPI * _CloseHandle)(HANDLE);
VOID   (WINAPI *_Sleep)(DWORD dwMilliseconds);
double (__cdecl *_atan2)(double, double);
double (__cdecl *_sqrt)(double);
double (__cdecl *_sin)(double);
double (__cdecl *_cos)(double);

typedef struct { float x, y, z; } vec3 ;

struct {
    uintptr_t client, entity;
    uintptr_t engine, cvar;
} vt ;

struct {
    int dwClientState,   dwEntityList,    dwGetLocalPlayer;
    int dwGetViewAngles, dwGetState,      m_iHealth;
    int m_lifeState,     m_vecViewOffset, m_iTeamNum;
    int m_vecPunch,      dwBoneMatrix,    m_vecOrigin;
} nv ;

struct crc32 { DWORD c, l, i; };
struct crc32 CRC32(DWORD c, DWORD l, DWORD i)
{
    struct crc32 crc;
    crc.c = c; crc.l = l; crc.i = i;
    return crc;
}

#define CRC32(c, l, i) ((struct crc32) { c, l, i })
#define CRC32_CMP(crc, t) RtlCrc32((const void *)t, strlen((const char *)t), (DWORD)crc.i) == (unsigned int)crc.c
#define CRC32_WCMP(crc, t) RtlCrc32((const void *)t, wcslen((const wchar_t *)t), (DWORD)crc.i) == (unsigned int)crc.c
#define CRC32_CMPO(crc, t, off) RtlCrc32((const void *)t, strlen((const char *)t) - off, (DWORD)crc.i) == (unsigned int)crc.c

uintptr_t get_module(struct crc32 crc)
{
    uintptr_t a0, a1;

    a0 = *(uintptr_t*)(*(uintptr_t*)(__readfsdword(0x30) + 0x0C) + 0x14);
    a1 = *(uintptr_t*)(a0 + 4);
    while (a0 != a1) {
        if (CRC32_WCMP(crc, *(const wchar_t**)(a0 + 0x28))) {
            return *(uintptr_t*)(a0 + 0x10);
        }
        a0 = *(uintptr_t*)(a0);
    }
    return 0;
}

uintptr_t get_export(uintptr_t module, struct crc32 crc)
{
    int a0;
    int a1[4];

    a0 = module + *(int*)(module + 0x3C);
    a0 = module + *(int*)(a0 + 0x78);
    memcpy(&a1, (const void*)(a0 + 0x18), sizeof(a1));
    while (a1[0]--) {
        a0 = *(int*)(module + a1[2] + (a1[0] * 4));
        if (CRC32_CMP(crc, (const char*)(module + a0))) {
            return (module + *(int*)(module + a1[1] + (*(short*)(module + a1[3] + (a1[0] * 2)) * 4)));
        }
    }
    return 0;
}

uintptr_t get_factory(struct crc32 crc)
{
    uintptr_t v = get_export(get_module(crc), CRC32(1505290531, 15, 101363432));
    return v != 0 ? *(uintptr_t*)(*(uintptr_t*)(v - 0x6A)) : 0;
}

uintptr_t get_interface(uintptr_t factory, struct crc32 crc)
{
    do {
        if (CRC32_CMPO(crc, *(const char**)(factory + 0x4), 3)) {
            return *(uintptr_t*)(*(uintptr_t*)(factory) + 1);
        }
    } while ((factory = *(uintptr_t*)(factory + 0x8)) != 0);
    return 0;
}

static uintptr_t get_function(uintptr_t vtable, int index) { return *(uintptr_t*)(*(uintptr_t*)(vtable) + index * 4); }

static uintptr_t get_table(struct crc32 crc)
{
    uintptr_t a0, a1;

    a0 = *(uintptr_t*)(*(uintptr_t*)(get_function(vt.client, 8) + 1));
    do {
        a1 = *(uintptr_t*)(a0 + 0xC);
        if (CRC32_CMP(crc, *(const char **)(a1 + 0xC)))
            return a1;
    } while((a0 = *(uintptr_t*)(a0 + 0x10)) != 0);
    return 0;
}

static int get_offset(uintptr_t table, struct crc32 crc)
{
    int a0 = 0, a1, a2, a3, a4, a5;

    for (a1 = 0; a1 < *(int*)(table + 0x4); a1++) {
        a2 = a1 * 60 + *(int*)(table);
        a3 = *(int*)(a2 + 0x2C);
        if ((a4 = *(int*)(a2 + 0x28)) != 0 && *(int*)(a4 + 0x4) != 0)
            if ((a5 = get_offset(a4, crc)) != 0)
                a0 += a3 + a5;
        if (CRC32_CMP(crc, *(const char**)(a2))) {
            return a3 + a0;
        }
    }
    return a0;
}

static uintptr_t get_cvar(struct crc32 crc)
{
    uintptr_t a1 = *(uintptr_t*)(*(uintptr_t*)(vt.cvar + 0x34));
    while ((a1 = *(uintptr_t*)(a1 + 0x4)) != 0)
        if (CRC32_CMP(crc, *(const char**)(a1 + 0xc)))
            return a1;
    return 0;
}

static int get_cvar_32(uintptr_t convar)
{
    return *(int*)(convar + 0x30) ^ convar;
}

static float get_cvar_32f(uintptr_t convar)
{
    int v = *(int*)(convar + 0x2C) ^ convar;
    return *(float*)&v;
}

static void set_cvar_32f(uintptr_t convar, float v)
{
    *(int*)(convar + 0x2C) = *(int*)&v ^ convar;
}

static BOOL isInGame(void)             { return *(BOOL*)(nv.dwClientState + nv.dwGetState) == 6; }
static int  getLocalPlayer(void)       { return *(int*)(nv.dwClientState + nv.dwGetLocalPlayer); }
static int  getClientEntity(int index) { return *(int*)(nv.dwEntityList + index * 0x10); }
static int  getTeam(int entity)        { return *(int*)(entity + nv.m_iTeamNum); }
static int  getHealth(int entity)      { return *(int*)(entity + nv.m_iHealth); }
static int  getLifeState(int entity)   { return *(int*)(entity + nv.m_lifeState); }
static int  getBoneMatrix(int entity)  { return *(int*)(entity + nv.dwBoneMatrix); }
static vec3 getVecOrigin(int entity)   { return *(vec3*)(entity + nv.m_vecOrigin); }
static vec3 getVecView(int entity)     { return *(vec3*)(entity + nv.m_vecViewOffset); }
static vec3 getVecPunch(int entity)    { return *(vec3*)(entity + nv.m_vecPunch); }
static vec3 getViewAngles(void)        { return *(vec3*)(nv.dwClientState + nv.dwGetViewAngles); }
static vec3 getEyePos(int entity)
{
    vec3 v, o;

    v = getVecView(entity);
    o = getVecOrigin(entity);
    return (vec3) { v.x + o.x, v.y + o.y, v.z + o.z } ;
}

static vec3 getBonePos(int entity, int index)
{
    vec3 v = { 0 };
    int  m;

    m = getBoneMatrix(entity);
    if (m == 0)
        return v;
    v.x = *(float*)(m + 0x30 * index + 0x0C);
    v.y = *(float*)(m + 0x30 * index + 0x1C);
    v.z = *(float*)(m + 0x30 * index + 0x2C);
    return v;
}

static BOOL isValid(int entity)
{
    int health;

    if (entity == 0)
        return 0;
    health = getHealth(entity);
    return entity && getLifeState(entity) == 0 && health > 0 && health < 1337;
}

static BOOL initialize_functions(void)
{
    uintptr_t m;
    if ((m = get_module(CRC32(1336618664, 9, 355701351))) == 0) return FALSE;                                /* ntdll.dll */
    if ((*(uintptr_t*)&_atan2 = get_export(m, CRC32(-1208413014, 5, 25061309))) == 0) return FALSE;          /* atan2 */
    if ((*(uintptr_t*)&_sqrt = get_export(m, CRC32(1089090901, 4, 2113851773))) == 0) return FALSE;          /* sqrt */
    if ((*(uintptr_t*)&_sin = get_export(m, CRC32(1156354345, 3, 1061462970))) == 0) return FALSE;           /* sin */
    if ((*(uintptr_t*)&_cos = get_export(m, CRC32(636348740, 3, 1064479962))) == 0) return FALSE;            /* cos */
    if ((m = get_module(CRC32(-1564104622, 12, 1180227225))) == 0) return FALSE;                             /* KERNEL32.DLL */
    if ((*(uintptr_t*)&_CreateThread = get_export(m, CRC32(-770929529, 12, 2097875415))) == 0) return FALSE; /* CreateThread */
    if ((*(uintptr_t*)&_CloseHandle  = get_export(m, CRC32(644799219, 11, 2089162550))) == 0) return FALSE;  /* CloseHandle */
    if ((*(uintptr_t*)&_Sleep = get_export(m, CRC32(871370551, 5, 114390764))) == 0) return FALSE;           /* Sleep */
    return TRUE;
}

static BOOL initialize_interfaces(void)
{
    int t;

    if ((t = get_factory(CRC32(765433668, 11, 1675198160))) == 0) return 0;                       /* vstdlib.dll */
    if ((vt.cvar = get_interface(t, CRC32(-1774926910, 11, 321091948))) == 0) return 0;           /* VEngineCvar */
    if ((t = get_factory(CRC32(-977266194, 10, 1879618954))) == 0) return 0;                      /* engine.dll */
    if ((vt.engine = get_interface(t, CRC32(-1563693526, 13, 21364416))) == 0) return 0;          /* VEngineClient */
    if ((t = get_factory(CRC32(1478622760, 19, 1548099251))) == 0) return 0;                      /* client_panorama.dll */
    if ((vt.entity = get_interface(t, CRC32(1789069159, 17, 74332196))) == 0) return 0;           /* VClientEntityList */
    if ((vt.client = get_interface(t, CRC32(1051283942, 7, 809333349))) == 0) return 0;           /* VClient */
    return 1;
}

static BOOL initialize_netvars(void)
{
    int t;

    nv.dwClientState    = *(int*)(*(int*)(get_function(vt.engine, 18) + 0x16));
    nv.dwEntityList     = vt.entity - (*(int*)(get_function(vt.entity, 5) + 0x22) - 0x38);
    nv.dwGetLocalPlayer = *(int*)(get_function(vt.engine, 12) + 0x16);
    nv.dwGetViewAngles  = *(int*)(get_function(vt.engine, 19) + 0xB2);
    nv.dwGetState       = *(int*)(get_function(vt.engine, 26) + 0x07);
    
    if ((t = get_table(CRC32(-1834292033, 16, 2143189635))) == 0) return 0;            /* DT_BaseAnimating */
        nv.dwBoneMatrix    = get_offset(t, CRC32(-1563350005, 12, 1532917158)) + 0x1C; /* m_nForceBone */
    if ((t = get_table(CRC32(-96230388, 13, 1368077207))) == 0) return 0;              /* DT_BaseEntity */
        nv.m_vecOrigin     = get_offset(t, CRC32(-673322472, 11, 979638726));          /* m_vecOrigin */
        nv.m_iTeamNum      = get_offset(t, CRC32(-1033337736, 10, 1136402645));        /* m_iTeamNum */
    if ((t = get_table(CRC32(2030591008, 13, 1519792398))) == 0) return 0;             /* DT_BasePlayer  */
        nv.m_iHealth       = get_offset(t, CRC32(-1956581322, 9, 1195260790));         /* m_iHealth */
        nv.m_lifeState     = get_offset(t, CRC32(211519899, 11, 759023442));           /* m_lifeState */
        nv.m_vecViewOffset = get_offset(t, CRC32(-1429466835, 18, 1044359072));        /* m_vecViewOffset[0] */
        nv.m_vecPunch      = get_offset(t, CRC32(-2054851689, 7, 34174687)) + 0x70;    /* m_Local */
    return 1;
}

static void main_thread(void);

BOOL initialize_everything(void)
{
    if (!initialize_functions())
        return FALSE;
    if (!initialize_interfaces())
        return FALSE;
    if (!initialize_netvars())
        return FALSE;
    if ((_mp_teammates_are_enemies = get_cvar(CRC32(354703522, 24, 1872518935))) == 0)
        return FALSE;
    if ((_volume = get_cvar(CRC32(-828087349, 6, 2122840634))) == 0)
        return FALSE;
    _start_volume = get_cvar_32f(_volume);
    _min_volume   = 1.0f - _start_volume;

    _CloseHandle(_CreateThread(0, 0, (PVOID)main_thread, 0, 0, 0));
    return TRUE;
}

#define RAD2DEG(x) ((float)(x) * (float)(180.f / 3.14159265358979323846f))
#define DEG2RAD(x) ((float)(x) * (float)(3.14159265358979323846f / 180.f))


static void sincos(float radians, float *sine, float *cosine)
{
    *sine = (float)_sin(radians);
    *cosine = (float)_cos(radians);
}

static void angle_vec(vec3 angles, vec3 *forward)
{
    float sp, sy, cp, cy;
    sincos(DEG2RAD(angles.x), &sp, &cp);
    sincos(DEG2RAD(angles.y), &sy, &cy);
    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

static void vec_clamp(vec3 *v)
{
    if ( v->x > 89.0f && v->x <= 180.0f ) {
        v->x = 89.0f;
    }
    if ( v->x > 180.0f ) {
        v->x = v->x - 360.0f;
    }
    if( v->x < -89.0f ) {
        v->x = -89.0f;
    }
    v->y = fmodf(v->y + 180, 360) - 180;
    v->z = 0;
}

static void vec_normalize(vec3 *vec)
{
    float radius;

    radius = 1.f / (float)(_sqrt(vec->x*vec->x + vec->y*vec->y + vec->z*vec->z) + 1.192092896e-07f);
    vec->x *= radius, vec->y *= radius, vec->z *= radius;
}

static void vec_angles(vec3 forward, vec3 *angles)
{
    float tmp, yaw, pitch;

    if (forward.y == 0.f && forward.x == 0.f) {
        yaw = 0;
        if (forward.z > 0) {
            pitch = 270;
        } else {
            pitch = 90.f;
        }
    } else {
        yaw = (float)(_atan2(forward.y, forward.x) * 180.f / 3.14159265358979323846f);
        if (yaw < 0) {
            yaw += 360.f;
        }
        tmp = (float)_sqrt(forward.x * forward.x + forward.y * forward.y);
        pitch = (float)(_atan2(-forward.z, tmp) * 180.f / 3.14159265358979323846f);
        if (pitch < 0) {
            pitch += 360.f;
        }
    }
    angles->x = pitch;
    angles->y = yaw;
    angles->z = 0.f;
}

static float vec_dot(vec3 v0, vec3 v1)
{
    return (v0.x * v1.x + v0.y * v1.y + v0.z * v1.z);
}

static float vec_length(vec3 v)
{
    return (v.x * v.x + v.y * v.y + v.z * v.z);
}

static float get_fov(vec3 vangle, vec3 angle)
{
    vec3 a0, a1;

    angle_vec(vangle, &a0);
    angle_vec(angle, &a1);
    return RAD2DEG(acos(vec_dot(a0, a1) / vec_length(a0)));
}

static vec3 get_target_angle(int self, int target, int hitbox_id)
{
    vec3 m, c;

    m = getBonePos(target, hitbox_id);
    c = getEyePos(self);  
    c.x = m.x - c.x; c.y = m.y - c.y; c.z = m.z - c.z;
    vec_normalize(&c);
    vec_angles(c, &c);
    m = getVecPunch(self);
    c.x -= m.x * 2.0f, c.y -= m.y * 2.0f, c.z -= m.z * 2.0f;
    vec_clamp(&c);
    return c;
}

static BOOL get_target(int self, vec3 vangle)
{
    float     best_fov;
    int       i;
    int       entity;
    float     fov;
    int       j;

    best_fov = 9999.0f;
    for (i = 1; i < 64; i++) {
        entity = getClientEntity(i);
        if (!isValid(entity))
            continue;
        if (get_cvar_32(_mp_teammates_are_enemies) == 0 && getTeam(self) == getTeam(entity))
            continue;
        for (j = 6; j-- ;) {
            fov = get_fov(vangle, get_target_angle(self, entity, hitbox_list[j]));
            if (fov < best_fov) {
                best_fov       = fov;
                _target        = entity;
                _target_hitbox = hitbox_list[j];
            }
        }
    }
    return best_fov != 9999.0f;
}

static void sound_esp(void)
{
    int   self;
    vec3  vangle;
    float fov, vol;
    
    self = getClientEntity(getLocalPlayer());
    if (self == 0)
        return;
    vangle = getViewAngles();
    if (!get_target(self, vangle)) {
        if (get_cvar_32f(_volume) != _start_volume)
            set_cvar_32f(_volume, _start_volume);
        return;
    }
    fov = get_fov(vangle, get_target_angle(self, _target, _target_hitbox));
    if (fov <= 6.0f) {
        vol = 1.0f - fov / 6.0f * _min_volume;
        if (vol > 1.0f || vol <= _start_volume)
            return;
        set_cvar_32f(_volume, vol);
    } else {
        if (get_cvar_32f(_volume) != _start_volume)
            set_cvar_32f(_volume, _start_volume);
    }
}

static void main_thread(void)
{
    while (1) {
        _Sleep(1);
        if (isInGame()) {
            sound_esp();
        }
    }
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD rea, LPVOID res)
{
    BOOL result = 1;
    if (rea == 1) {
        result = initialize_everything();
    }
    return result;
}
