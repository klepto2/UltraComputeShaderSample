#pragma once
#include "UltraEngine.h"

using namespace UltraEngine;

class CameraControls : public Component
{
public:
    bool freelookstarted = false;
	float mousesmoothing = 0.0f;
	float mouselookspeed = 1.0f;
	float movespeed = 4.0f;
	Vec3 freelookmousepos;
	Vec3 freelookrotation;
	Vec2 lookchange;

	virtual void Update()
    {
		auto window = ActiveWindow();
		if (window == NULL) return;
		if (!freelookstarted)
		{
			freelookstarted = true;
			freelookrotation = entity->GetRotation(true);
			freelookmousepos = window->GetMouseAxis();
		}
		auto newmousepos = window->GetMouseAxis();
		lookchange.x = lookchange.x * mousesmoothing + (newmousepos.y - freelookmousepos.y) * 100.0f * mouselookspeed * (1.0f - mousesmoothing);
		lookchange.y = lookchange.y * mousesmoothing + (newmousepos.x - freelookmousepos.x) * 100.0f * mouselookspeed * (1.0f - mousesmoothing);
		if (Abs(lookchange.x) < 0.001f) lookchange.x = 0.0f;
		if (Abs(lookchange.y) < 0.001f) lookchange.y = 0.0f;
		if (lookchange.x != 0.0f or lookchange.y != 0.0f)
		{
			freelookrotation.x += lookchange.x;
			freelookrotation.y += lookchange.y;
			entity->SetRotation(freelookrotation, true);
		}
		freelookmousepos = newmousepos;
		float speed = movespeed / 60.0f;
		if (window->KeyDown(KEY_SHIFT))
		{
			speed *= 10.0f;
		}
		else if (window->KeyDown(KEY_CONTROL))
		{
			speed *= 0.25f;
		}
		if (window->KeyDown(KEY_E)) entity->Translate(0, speed, 0);
		if (window->KeyDown(KEY_Q)) entity->Translate(0, -speed, 0);
		if (window->KeyDown(KEY_D)) entity->Move(speed, 0, 0);
		if (window->KeyDown(KEY_A)) entity->Move(-speed, 0, 0);
		if (window->KeyDown(KEY_W)) entity->Move(0, 0, speed);
		if (window->KeyDown(KEY_S)) entity->Move(0, 0, -speed);
    }

	//This method will work with simple components
	virtual shared_ptr<Component> Copy()
	{
		return std::make_shared<CameraControls>(*this);
	}
};