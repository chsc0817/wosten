
#if !defined DEFINES_H
#define DEFINES_H

#define  u8 uint8_t
#define u32 uint32_t
#define u64 uint64_t
#define f32 float
#define s32 int32_t 
#define s64 int64_t 
#define usize size_t

#define assert(condition) { \
    if (!(condition)) {\
        printf("assert failed: %s \n", #condition); \
        *((u32 *) NULL) = 123; \
    } \
}

#define ABS(a)   (a >= 0 ? a : -a)
#define MAX(a,b) (a > b ? a : b)
#define MIN(a,b) (a < b ? a : b)
#define CLAMP(a, min, max) ((min > a) ? min : (max < a ? max : a))

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))
#define ARRAY_WITH_COUNT(array) array, ARRAY_COUNT(array)

#define FLAG(bit) (1 << bit)

#define PI 3.14159265359

struct vec2 {
    f32 x, y;
};


//vector operations
vec2 operator* (vec2 a, f32 scale) {
    vec2 result;
    result.x = a.x * scale;
    result.y = a.y * scale;
    
    return result;
}

vec2 operator* (vec2 a, vec2 b) {
    vec2 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    
    return result;
}

vec2 operator+ (vec2 a, vec2 b) {
    vec2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    
    return result;
}

vec2 operator- (vec2 a, vec2 b) {
    vec2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    
    return result;
}

vec2 operator- (vec2 a) {
    vec2 result;
    result.x = -a.x;
    result.y = -a.y;
    
    return result;
}


f32 dot(vec2 a, vec2 b) {
    return (a.x * b.x + a.y * b.y);
}


f32 lengthSquared(vec2 a) {
	return dot(a, a);
}


f32 length(vec2 a) {
    return sqrt(lengthSquared(a));
}

vec2 normalizeOrZero(vec2 a) {
    if ((a.x == 0) && (a.y == 0))
        return {};
    
    vec2 result;
    
    result.x = a.x * 1 / (length(a));
    result.y = a.y * 1 / (length(a));
    
    return result;
}

struct transform {
    vec2 pos;
    f32 rotation;
    f32 scale;
};

vec2 transformPoint(transform t, vec2 point, f32 heightOverWidth) {
    vec2 result;
    f32 cosRotation = cos(t.rotation);
    f32 sinRotation = sin(t.rotation);
    
    result.x = (cosRotation * point.x - sinRotation * point.y) * t.scale + t.pos.x;
    result.y = (sinRotation * point.x + cosRotation * point.y) * t.scale + t.pos.y;
    
    result.x *= heightOverWidth;
    
    return result;
}

f32 lerp(f32 a, f32 b, f32 t) {
    f32 result = (a * (1 - t)) + (b * t);
    return result;
}

//geometry
struct circle{
	vec2 pos;
	f32 radius;
};

bool areIntersecting(circle a, circle b) {
    f32 combinedRadius = a.radius + b.radius;
    vec2 distance = a.pos - b.pos;
    
    return (combinedRadius * combinedRadius >= lengthSquared(distance));
}


#endif // DEFINES_H