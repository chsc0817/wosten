
#if !defined DEFINES_H
#define DEFINES_H

// FLT_MAX
#include <cfloat> 

#define  u8 uint8_t
#define u32 uint32_t
#define u64 uint64_t

#define s32 int32_t 
#define s64 int64_t 

#define usize size_t

#define f32 float

#define f32_min -FLT_MAX
#define f32_max  FLT_MAX

#define assert(condition) { \
    if (!(condition)) {\
        printf("assert failed: %s \n", #condition); \
        *((u32 *) NULL) = 123; \
    } \
}

#define ABS(a)   ((a) >= 0 ? (a) : -(a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define CLAMP(a, min, max) (((min) > (a)) ? (min) : ((max) < (a) ? (max) : (a)))

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define ARRAY_WITH_COUNT(array) (array), ARRAY_COUNT(array)

#define FLAG(bit) (1 << (bit))

#define PI 3.14159265359

struct vec2 {
    f32 X, Y;
};

union rect
{
    struct
    {
        vec2 BottomLeft;
        vec2 TopRight;
    };

    struct
    {
        f32 Left, Bottom;
        f32 Right, Top;
    };
};

rect MakeRect(f32 Left, f32 Bottom, f32 Right, f32 Top) {
    rect Result;
    Result.BottomLeft = {Left, Bottom};
    Result.TopRight = {Right, Top};
    
    return Result;
}

rect MakeRectWithSize(f32 Left, f32 Bottom, f32 Width, f32 Height) {
    rect Result;
    Result.BottomLeft = {Left, Bottom};
    Result.TopRight   = {Left + Width, Bottom + Height};
    
    return Result;
}


rect MakeRect(vec2 BottomLeft, vec2 TopRight) {
    rect Result;
    Result.BottomLeft = BottomLeft;
    Result.TopRight = TopRight;
    
    return Result;
}

rect MakeEmptyRect()
{
    rect Result;
    Result.Left   = f32_max;
    Result.Bottom = f32_max;
    Result.Right  = f32_min;
    Result.Top    = f32_min;
    return Result;
}

bool IsValid(rect Result)
{
    return ((Result.Left != f32_max) &&
        (Result.Bottom != f32_max) &&
        (Result.Right != f32_min) &&
        (Result.Top != f32_min));
}

rect Merge(rect A, rect B) {
    rect Result;
    Result.BottomLeft.X = MIN(A.BottomLeft.X, B.BottomLeft.X);
    Result.BottomLeft.Y = MIN(A.BottomLeft.Y, B.BottomLeft.Y);
    Result.TopRight.X = MAX(A.TopRight.X, B.TopRight.X);
    Result.TopRight.Y = MAX(A.TopRight.Y, B.TopRight.Y);
    
    return Result;
}

bool Contains(rect Rect, vec2 Point)
{
    return ((Rect.BottomLeft.X <= Point.X) && (Point.X <= Rect.TopRight.X) && (Rect.BottomLeft.Y <= Point.Y) && (Point.Y <= Rect.TopRight.Y));      
}

//vector operations
vec2 operator* (vec2 A, f32 Scale) {
    vec2 Result;
    Result.X = A.X * Scale;
    Result.Y = A.Y * Scale;
    
    return Result;
}

vec2 operator* (vec2 A, vec2 B) {
    vec2 Result;
    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    
    return Result;
}

vec2 operator+ (vec2 A, vec2 B) {
    vec2 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    
    return Result;
}


vec2 operator+ (vec2 A, f32 B) {
    vec2 Result;
    Result.X = A.X + B;
    Result.Y = A.Y + B;
    
    return Result;
}

vec2 operator- (vec2 A, vec2 B) {
    vec2 Result;
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    
    return Result;
}

vec2 operator- (vec2 A) {
    vec2 Result;
    Result.X = -A.X;
    Result.Y = -A.Y;
    
    return Result;
}


f32 dot(vec2 A, vec2 B) {
    return (A.X * B.X + A.Y * B.Y);
}


f32 lengthSquared(vec2 A) {
	return dot(A, A);
}


f32 length(vec2 A) {
    return sqrt(lengthSquared(A));
}

vec2 normalizeOrZero(vec2 A) {
    if ((A.X == 0) && (A.Y == 0))
        return {};
    
    vec2 Result;
    
    Result.X = A.X * 1 / (length(A));
    Result.Y = A.Y * 1 / (length(A));
    
    return Result;
}

struct transform {
    vec2 Pos;
    f32 Rotation;
    f32 Scale;
};

vec2 TransformPoint(transform T, vec2 Point) {
    vec2 Result;
    f32 CosRotation = cos(T.Rotation);
    f32 SinRotation = sin(T.Rotation);
    
    Result.X = (CosRotation * Point.X - SinRotation * Point.Y) * T.Scale + T.Pos.X;
    Result.Y = (SinRotation * Point.X + CosRotation * Point.Y) * T.Scale + T.Pos.Y;
    
    return Result;
}

f32 lerp(f32 A, f32 B, f32 T) {
    f32 Result = (A * (1 - T)) + (B * T);
    return Result;
}

//geometry
struct circle{
	vec2 Pos;
	f32 Radius;
};

bool areIntersecting(circle A, circle B) {
    f32 CombinedRadius = A.Radius + B.Radius;
    vec2 Distance = A.Pos - B.Pos;
    
    return (CombinedRadius * CombinedRadius >= lengthSquared(Distance));
}


#endif // DEFINES_H