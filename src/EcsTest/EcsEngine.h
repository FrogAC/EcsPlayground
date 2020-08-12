/*
Ref:  https://austinmorlan.com/posts/entity_component_system/
      https://tsprojectsblog.wordpress.com/portfolio/entity-component-system/

Head-only ECS lib
*/
#ifndef ECS_ENGINE_H_
#define ECS_ENGINE_H_

#include <cassert>
#include <cstdint>
#include <array>
#include <bitset>
#include <queue>
#include <unordered_map>
#include <set>
#include <typeinfo>
#include "Core/Main.h"
#include "Core/Assertion.h"
using namespace std;

namespace Ecs {

// ecs_common.h
//----------------------------------------------------------------

using EntityId_T = uint32_t;
const EntityId_T MAX_ENTITY = 1000;

using ComponentId_T = uint8_t;
const ComponentId_T MAX_COMPONENT = 32;

using SystemId_T = uint8_t;
// no bitset nor compact addresses needed for systems, thus no MAX_SYSTEM

using Signature_T = bitset<MAX_COMPONENT>;

// ecs_engine.h
//----------------------------------------------------------------
class EcsEngine {
    public:
        //----------------------------------------------------------------
        // Singleton
        //----------------------------------------------------------------
        static EcsEngine& GetInstance() {
            static EcsEngine instance;
            return instance;
        }

        EcsEngine(EcsEngine const&) = delete;
        void operator=(EcsEngine const&) = delete;

        //----------------------------------------------------------------
        // Functions
        //----------------------------------------------------------------
        EntityId_T CreateEntity() {
            return mEntityManager.CreateEntity();
        }

        void DestroyEntity(EntityId_T entity) {
            mEntityManager.DestroyEntity(entity);
            mSystemManager.OnEntityDestroy(entity);
        }

        template <typename T>
        void ResisterComponent() {
        }

        template <typename T>
        void AddComponent(EntityId_T entity, T component) {
        }

        template <typename T>
        void RemoveComponent(EntityId_T entity, T component) {

        }

        template <typename T>
        T& GetComponent(EntityId_T entity) {

        }

    private:
        Internal::EntityManager mEntityManager;
        Internal::ComponentManager mComponentManager;
        Internal::SystemManager mSystemManager;

        EcsEngine() {}
};

namespace Internal {

// ecs_entity.h
//----------------------------------------------------------------
/* 
EntityManager:
    1) entity pool
    2) Create/Destroy Entities
    3) Store Signature
*/
class EntityManager {
    public:
        EntityManager() {
            mEntityCount = 0;
            mAvailiableEntities = queue<EntityId_T>();
            mEntityUsage.reset();

            for (auto i = 0 ; i < MAX_ENTITY ; i++) {
                mAvailiableEntities.push(i);
            }
        }

        EntityId_T CreateEntity() {
            EntityId_T entity = mAvailiableEntities.front();
            o_assert_dbg(!mEntityUsage[entity] && "entity in use");

            mEntityUsage[entity] = true;
            mAvailiableEntities.pop();
            mEntityCount++;

            // init Signature
            mSignatures[entity].reset();

            return entity;
        }

        void DestroyEntity(EntityId_T entity) {
            o_assert_dbg(mEntityUsage[entity] && "entity not in use");

            mEntityUsage[entity] = false;
            mAvailiableEntities.push(entity);
            mEntityCount--;
        }

        Signature_T GetSignature(EntityId_T entity) const{
            o_assert_dbg(mEntityUsage[entity] && "entity not in use");

            return mSignatures[entity];
        }

        void SetSignature(EntityId_T entity, Signature_T signature) {
            o_assert_dbg(mEntityUsage[entity] && "entity not in use");

            mSignatures[entity] = signature;
        }

        EntityId_T Size() const { return mEntityCount; }

    private:
        bitset<MAX_ENTITY> mEntityUsage;
        queue<EntityId_T> mAvailiableEntities;
        // component list
        array<Signature_T, MAX_ENTITY> mSignatures;
        EntityId_T mEntityCount;
};


// ecs_component.h
//----------------------------------------------------------------
/*
IComponentArray:
    Tow designs on clean up.
    A: EttManager.GetSignature() 
        => ComponentManager.RemoveComponent(entity, typeid) for all typeid
    B: ComponentManager.OnEntityDestroy()
        => ComponentArray<T>.OnEntityDestroy() for all T
    A sounds better
*/
class IComponentArray {
    

    public:
        virtual ~IComponentArray() = default;
        virtual void RemoveComponent(EntityId_T entity) = 0;
};

/*
ComponentArray<T>:
    maintain components of type T. Know relative eneity ids
*/
template <typename T>
class ComponentArray : public IComponentArray {
    public:
        ComponentArray() {
            // mDataArray = array<T, MAX_ENTITY>();
            // mId2Entity = array<EntityId_T, MAX_ENTITY>();
            mEntity2Id = array<EntityId_T, MAX_ENTITY>();
            mEntity2Id.fill(MAX_ENTITY);
            mSize = 0;
        }

        void AddComponent(EntityId_T entity, T component) {
            o_assert_dbg(entity < MAX_ENTITY && "entity out of range");
            o_assert_dbg(mEntity2Id[entity] == MAX_ENTITY && "entity exist");

            // attach to last
            mDataArray[mSize] = move(component);
            // set id
            mEntity2Id[entity] = mSize;
            // set entity
            mId2Entity[mSize] = entity;

            mSize++;
        }

        void RemoveComponent(EntityId_T entity) override {
            o_assert_dbg(entity < MAX_ENTITY && "entity out of range");
            o_assert_dbg(mEntity2Id[entity] < mSize && "entity not exist");

            mSize--;

            // move last data to fill the gap, update id
            EntityId_T gapId = mEntity2Id[entity];
            mDataArray[gapId] = mDataArray[mSize];
            // update last data's id <-> entity
            mEntity2Id[mId2Entity[mSize]] = gapId;
            mId2Entity[gapId] = mId2Entity[mSize];
            // init removed entity id
            mEntity2Id[entity] = MAX_ENTITY;
        }

        T& GetComponent(EntityId_T entity) {
            o_assert_dbg(entity < MAX_ENTITY && "entity out of range");
            o_assert_dbg(mEntity2Id[entity] < mSize && "entity not exist");

            return mDataArray[mEntity2Id[entity]];
        }

        EntityId_T Size() {
            return mSize;
        }

    private:
        EntityId_T mSize;

        array<T, MAX_ENTITY> mDataArray;
        array<EntityId_T, MAX_ENTITY> mEntity2Id;
        array<EntityId_T, MAX_ENTITY> mId2Entity;
};


/*
ComponentManager:
    maintain ComponentArray with different type

    index ComponentArray<T> using string pointer of T type_info::name
    ALl component has to be registed on init.
*/
class ComponentManager {
    public:
        ComponentManager() {
            mSize = 0;
        }

        template <typename T>
        void RegisterComponent() {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) == mName2Id.end() && "Component Registered");            
            o_assert_dbg(mSize < MAX_COMPONENT && "Max Component Reached");

            mName2Id[name] = mSize;
            mId2Array[mSize] = make_shared<ComponentArray<T>>();

            mSize++;
        }

        template <typename T>
        void AddComponent(EntityId_T entity, T component) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            static_pointer_cast<ComponentArray<T>>(mId2Array[mName2Id[name]])->AddComponent(entity, component);
        }
        
        template <typename T>
        void RemoveComponent(EntityId_T entity) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            static_pointer_cast<ComponentArray<T>>(mId2Array[mName2Id[name]])->RemoveComponent(entity);
        }

        void RemoveComponent(EntityId_T entity, ComponentId_T componentId) {
            o_assert_dbg(componentId < mSize && "Component Not Registered");

            mId2Array[componentId]->RemoveComponent(entity);
        }

        void RemoveAllComponents(EntityId_T entity, Signature_T signature) {
            for (size_t i = 0; i < mSize; ++i) {
                if (signature.test(i))
                    mId2Array[i]->RemoveComponent(entity);
            }
        }

        template <typename T>
        T& GetComponent(EntityId_T entity) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            return static_pointer_cast<ComponentArray<T>>(mId2Array[mName2Id[name]])->GetComponent(entity);
        }

        ComponentId_T Size() const { return mSize; }
        
        template <typename T>
        ComponentId_T GetComponentId() const {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            return mName2Id[name];
        }
    private:
        ComponentId_T mSize;
        unordered_map<const char *, ComponentId_T> mName2Id;
        array<shared_ptr<IComponentArray>, MAX_COMPONENT> mId2Array;
};


// ecs_system.h
// ----------------------------------------------------------------

class System {
    friend class SystemManager;

    // Austin says set is faster
    set<EntityId_T> mEntities;
    const Signature_T mSignature;
};

/*
SystemManager
    Maintain Systems and entites list in each system

    Usually you want an empty component dedicate to each system
*/
class SystemManager {
    public:
        SystemManager() {}

        template <typename T>
        void RegisterSystem(Signature_T signature) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2System.find(name) == mName2Id.end() && "System Registered");

            mName2System[name] = make_shared<T>();
        }
        

        void OnEntityDestroy(EntityId_T entity) {
            // remove from all systems
            for (auto const& pair : mName2System) {
                shared_ptr<System> system = pair.second;
                system->mEntities.erase(entity);
            }
        }

        void OnEntitySignatureUpdate(EntityId_T entity, Signature_T signature) {
            // validate all systems
            for (auto const& pair : mName2System) {
                shared_ptr<System> system = pair.second;
                if ((signature & system->mSignature) == system->mSignature) {
                    system->mEntities.insert(entity);
                } else {
                    system->mEntities.erase(entity);
                }
            }
        }

        int Size() const {return mName2System.size();}
    private:
        unordered_map<const char *, shared_ptr<System>> mName2System;
};

}
}

#endif  // ECS_ENGINE_H_