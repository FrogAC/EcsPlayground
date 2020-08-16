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
namespace Internal {
    class SystemManager;  // forward for friend declaration in System
}
using namespace Internal;

// ecs_common.h
//----------------------------------------------------------------
using EntityId_T = uint32_t;
const EntityId_T MAX_ENTITY = 1000;

using ComponentId_T = uint8_t;
const ComponentId_T MAX_COMPONENT = 128;

using Signature_T = bitset<MAX_COMPONENT>;

class System {
    public:
        /**
         *  Please set mSignature correctly
         **/
        virtual void OnSystemRegister() = 0;
        virtual void Update() = 0;


    protected:
        set<EntityId_T> mEntities;
        Signature_T mSignature;

    friend class SystemManager;
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
        ComponentId_T AddComponent(EntityId_T entity, T component) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            ComponentId_T id = mName2Id[name];
            static_pointer_cast<ComponentArray<T>>(mId2Array[id])->AddComponent(entity, component);

            return id;
        }
        
        template <typename T>
        ComponentId_T RemoveComponent(EntityId_T entity) {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2Id.find(name) != mName2Id.end() && "Component Not Registered");

            ComponentId_T id = mName2Id[name];
            static_pointer_cast<ComponentArray<T>>(mId2Array[id])->RemoveComponent(entity);

            return id;
        }

        // void RemoveComponent(EntityId_T entity, ComponentId_T componentId) {
        //     o_assert_dbg(componentId < mSize && "Component Not Registered");

        //     mId2Array[componentId]->RemoveComponent(entity);
        // }

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

            return mName2Id.at(name);
        }

    private:
        ComponentId_T mSize;
        unordered_map<const char *, ComponentId_T> mName2Id;
        array<shared_ptr<IComponentArray>, MAX_COMPONENT> mId2Array;
};


// ecs_system.h
// ----------------------------------------------------------------


/*
SystemManager
    Maintain Systems and entites list in each system
*/
class SystemManager {
    public:
        SystemManager() {}

        template <typename T>
        shared_ptr<T> RegisterSystem() {
            const char* name = typeid(T).name();
            static_assert(std::is_base_of<System, T>::value, "T not derived from System");
            o_assert_dbg(mName2System.find(name) == mName2System.end() && "System Registered");

            auto ptr = make_shared<T>();
            mName2System[name] = static_pointer_cast<System>(ptr);
            ptr->OnSystemRegister();
            return ptr;
        }
        

        void OnEntityDestroy(EntityId_T entity) {
            // remove from all systems
            for (auto const& pair : mName2System) {
                shared_ptr<System> system = pair.second;
                system->mEntities.erase(entity);
            }
        }

        void OnEntitySignatureUpdate(EntityId_T entity, Signature_T const &signature) {
            // validate all systems
            for (auto const &pair : mName2System) {
                shared_ptr<System> system = pair.second;
                if ((signature & system->mSignature) == system->mSignature) {
                    system->mEntities.insert(entity);
                } else {
                    system->mEntities.erase(entity);
                }
            }
        }

        template <typename T>
        shared_ptr<T> GetSystem() {
            const char* name = typeid(T).name();
            o_assert_dbg(mName2System.find(name) != mName2System.end() && "System Not Registerd");

            return mName2System[name];
        }

        size_t Size() const {
            return mName2System.size();
        }
    private:
        unordered_map<const char *, shared_ptr<System>> mName2System;
};

} // namespace Internal

// ecs_engine.h
//----------------------------------------------------------------
class EcsEngine {
    public:
        //----------------------------------------------------------------
        // Singleton
        //----------------------------------------------------------------
        EcsEngine(EcsEngine const&) = delete;
        void operator=(EcsEngine const&) = delete;

        static EcsEngine& GetInstance() {
            static EcsEngine instance;
            return instance;
        }

        //----------------------------------------------------------------
        // Functions
        //----------------------------------------------------------------
        
        EntityId_T CreateEntity() {
            return mEntityManager->CreateEntity();
        }

        void DestroyEntity(EntityId_T entity) {
            Signature_T signature = mEntityManager->GetSignature(entity);
            mComponentManager->RemoveAllComponents(entity, move(signature));
            mSystemManager->OnEntityDestroy(entity);
            mEntityManager->DestroyEntity(entity);
        }

        // ----------------------------------------------------------------

        template <typename T>
        void ResisterComponent() { 
            mComponentManager->RegisterComponent<T>();
        }

        template <typename T>
        void AddComponent(EntityId_T entity, T component) {
            auto id = mComponentManager->AddComponent<T>(entity, move(component));
            auto signature = mEntityManager->GetSignature(entity);
            signature.set(id, true);
            mSystemManager->OnEntitySignatureUpdate(entity, signature);
            mEntityManager->SetSignature(entity, move(signature));
        }

        template <typename T>
        void RemoveComponent(EntityId_T entity, T component) {
            auto id = mComponentManager->RemoveComponent<T>(entity);
            auto signature = mEntityManager->GetSignature(entity);
            signature.set(id, false);
            mSystemManager->OnEntitySignatureUpdate(entity, signature);
            mEntityManager->SetSignature(entity, move(signature));
        }

        template <typename T>
        T& GetComponent(EntityId_T entity) {
            return mComponentManager->GetComponent<T>(entity);
        }

        template <typename T>
        ComponentId_T GetComponentId() const { 
            return mComponentManager->GetComponentId<T>();
        }

        // ---------------------------------------------------------------------

        template <typename T>
        shared_ptr<T> ResisterSystem() {
            shared_ptr<T> system = mSystemManager->RegisterSystem<T>();
            static_pointer_cast<System>(system)->OnSystemRegister();
            return system;
        }

        template <typename T>
        shared_ptr<T> GetSystem() {
            return mSystemManager->GetSystem<T>();
        }

    private:
        unique_ptr<EntityManager> mEntityManager;
        unique_ptr<ComponentManager> mComponentManager;
        unique_ptr<SystemManager> mSystemManager;

        EcsEngine() {
            mEntityManager = make_unique<EntityManager>();
            mComponentManager = make_unique<ComponentManager>();
            mSystemManager = make_unique<SystemManager>();
        }
};

} // namespace Ecs


#endif  // ECS_ENGINE_H_