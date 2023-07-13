#pragma once
#include "UltraEngine.h"

using namespace UltraEngine;

class Mover : public Component
{
public: 
     
    Vec3 movementspeed;
    Vec3 rotationspeed;
    bool globalcoords;
    
    Mover::Mover()
    { 
        globalcoords = false;
    }

    virtual void Update()
    {
        if (globalcoords)
        {
            this->entity->Translate(movementspeed / 60.0f, true);
        }
        else
        {
            this->entity->Move(movementspeed / 60.0f);
        }
        this->entity->Turn(rotationspeed / 60.0f, globalcoords);
    }

    //This method will work with simple components
    virtual shared_ptr<Component> Copy()
    {
        return std::make_shared<Mover>(*this);
    }
}; 