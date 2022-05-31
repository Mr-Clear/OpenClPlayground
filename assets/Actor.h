#pragma once

struct Actor
{
    float2 pos;
    float speed;
    float targetSpeed;
    float direction;
    bool alive;
    char _1[0]; // Fix alignment
};
