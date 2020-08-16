#define CATCH_CONFIG_MAIN
#include "catch.h"
#include "EcsEngine.h"
#include <array>

// ----------------------------------------------------------------
// Custom Matchers
// ----------------------------------------------------------------
// template<typename T, size_t N>
// class ArrayMatcher : public Catch::MatcherBase<std::array<T,N>> {
//     std::array<T, N> m_array;
// public:
//     ArrayMatcher(std::array<T,N> array) : m_array(array){}

//     bool match( std::array<T,N> const& target ) const override {
//         bool match = true;
//         for (int i = 0; match && i < N; ++i) {
//             match &= target[i] == m_array[i];
//         }
//         return match;
//     }

//     virtual std::string describe() const override {
//         std::ostringstream ss;
//         ss << "match " << m_array;
//         return ss.str();
//     }
// };

template <typename T, size_t N>
bool CompareArray(std::array<T, N> a, std::array<T, N> b) {
    bool match = true;
    for (int i = 0; match && i < N; ++i) {
        match &= a[i] == b[i];
    }
    return match;
}

// ----------------------------------------------------------------
// EntityManager
// ----------------------------------------------------------------

TEST_CASE( "verify EntityManager", "[ecs]" ) {
    Ecs::Internal::EntityManager entityManager;

    REQUIRE( entityManager.Size() == 0 );

    SECTION( "Create and Destroy change size" ) {
        auto ett0 = entityManager.CreateEntity();
        REQUIRE( entityManager.Size() == 1 );
        auto ett1 = entityManager.CreateEntity();
        REQUIRE( entityManager.Size() == 2 );
        auto ett2 = entityManager.CreateEntity();
        REQUIRE( entityManager.Size() == 3 );
        entityManager.DestroyEntity(ett2);
        REQUIRE( entityManager.Size() == 2 );
        entityManager.DestroyEntity(ett1);
        REQUIRE( entityManager.Size() == 1 );
        ett1 = entityManager.CreateEntity();
        REQUIRE( entityManager.Size() == 2 );
        entityManager.DestroyEntity(ett1);
        REQUIRE( entityManager.Size() == 1 );
    }

    SECTION( "verify Get and Set Signature" ) {
        auto ett0 = entityManager.CreateEntity();
        auto ett1 = entityManager.CreateEntity();

        // get init
        Ecs::Signature_T sig0 = entityManager.GetSignature(ett0);
        Ecs::Signature_T sig1 = entityManager.GetSignature(ett1);
        REQUIRE( sig0.none() );
        REQUIRE( sig1.none() );

        // change and set
        sig0.set(0,true);
        sig1.set(1,true);
        entityManager.SetSignature(ett0, sig0);
        entityManager.SetSignature(ett1, sig1);

        sig0 = entityManager.GetSignature(ett0);
        sig1 = entityManager.GetSignature(ett1);
        REQUIRE( sig0[0] );
        REQUIRE( sig1[1] );

        // reset
        entityManager.DestroyEntity(ett0);
        ett0 = entityManager.CreateEntity();
        sig0 = entityManager.GetSignature(ett0);
        REQUIRE( sig0.none() );
    }
}

// ----------------------------------------------------------------
// ComponentArray
// ----------------------------------------------------------------

TEST_CASE( "verify ComponentArray" , "[ecs]") {
    struct CT {
        int a;
        string b;
        std::array<int,3> c;
    };

    Ecs::Internal::ComponentArray<CT> ca;
    
    REQUIRE( ca.Size() == 0 );

    SECTION("Simple Add/Remove/Get") {
        // add ett 1 10
        ca.AddComponent(1, {0, "first", {0,1,2}});
        REQUIRE( ca.Size() == 1);
        ca.AddComponent(10, {1, "second", {3,4,5}});
        REQUIRE( ca.Size() == 2 );

        // remove 1
        ca.RemoveComponent(1);
        REQUIRE( ca.Size() == 1 );

        // check 10
        CT& data = ca.GetComponent(10);
        REQUIRE( data.a == 1 );
        REQUIRE_THAT(data.b, Catch::Equals("second"));
        REQUIRE(CompareArray(data.c, {3,4,5}));

        // add 1
        ca.AddComponent(1, {2, "third", {6,7,8}});
        REQUIRE( ca.Size() == 2);

        // check 1
        data = ca.GetComponent(1);
        REQUIRE( data.a == 2 );
        REQUIRE_THAT(data.b, Catch::Equals("third"));
        REQUIRE(CompareArray(data.c, {6,7,8}));

        // remove all elements
        ca.RemoveComponent(1);
        ca.RemoveComponent(10);
        REQUIRE(ca.Size() == 0);
    }

    SECTION("Repeat Add/Remove") {
        ca.AddComponent(1, {0, "first", {0,1,2}});
        ca.AddComponent(10, {1, "second", {3,4,5}});
        ca.AddComponent(100, {2, "third", {6,7,8}});
        REQUIRE( ca.Size() == 3 );

        ca.RemoveComponent(100);
        ca.RemoveComponent(10);
        REQUIRE( ca.Size() == 1 );
        CT &data = ca.GetComponent(1);
        REQUIRE( data.a == 0 );

        ca.AddComponent(100, {3, "third", {6,7,8}});
        ca.AddComponent(10, {4, "second", {3,4,5}});
        REQUIRE( ca.Size() == 3 );
        data = ca.GetComponent(10);
        REQUIRE( data.a == 4 );
        data = ca.GetComponent(100);
        REQUIRE( data.a == 3 );


        ca.RemoveComponent(100);
        ca.RemoveComponent(10);
        REQUIRE( ca.Size() == 1 );
    }

}

// ----------------------------------------------------------------
// ComponentManager
// ----------------------------------------------------------------

TEST_CASE( "verify ComponentManager" , "[ecs]") {
    struct A { int i; string s; };
    struct B { int i; float f; };

    Ecs::Internal::ComponentManager manager;
    REQUIRE( manager.Size() == 0 );

    // register
    manager.RegisterComponent<A>();
    manager.RegisterComponent<B>();
    REQUIRE( manager.Size() == 2 );

    SECTION("Max Size Add/Remove/Get") {
        int maxcount = (int)Ecs::MAX_ENTITY;
        for (int i = 0; i < maxcount; ++i) {
            manager.AddComponent<A>(i, {i, "NonSense"});
            manager.AddComponent<B>(i, {i, 4.2f});
        }

        REQUIRE( manager.GetComponent<A>(maxcount-1).i == maxcount-1);
        REQUIRE( manager.GetComponent<B>(maxcount-1).i == maxcount-1);

        // remove A with even id, B with odd id
        for (int i = 0; i < maxcount; i+=2)
            manager.RemoveComponent<A>(i);
        for (int i = 1; i < maxcount; i+=2)
            manager.RemoveComponent<B>(i);

        auto& TA = manager.GetComponent<A>(3);  
        REQUIRE( TA.i == 3);
        REQUIRE( manager.GetComponent<B>(12).i == 12);

        // allowing stale data
        manager.RemoveComponent<A>(3);
    }
}

// ----------------------------------------------------------------
// EcsEngine
// ----------------------------------------------------------------

TEST_CASE( "verify EcsEngine" , "[ecs]") {
    using namespace Ecs;
    static EcsEngine& ecs = EcsEngine::GetInstance();

    struct IntComponent { int i; };
    struct FloatComponent { float f; };

    struct IntInc : public Ecs::System {
        void OnSystemRegister() override {
            REQUIRE(mSignature.none());
            mSignature.set((size_t)ecs.GetComponentId<IntComponent>(),true);
        }
        void Update() override {
            for (const auto &entity : mEntities) {
                IntComponent& component =  ecs.GetComponent<IntComponent>(entity);
                component.i += 1;
            }
        }
    };
    struct FloatInc : public Ecs::System {
        void OnSystemRegister() override {
            REQUIRE(mSignature.none());
            mSignature.set(ecs.GetComponentId<FloatComponent>(),true);
        }
        void Update() override {
            for (const auto &entity : mEntities) {
                FloatComponent& component = ecs.GetComponent<FloatComponent>(entity);
                component.f += .1f;
            }
        }
    };
    struct NumMul : public Ecs::System {
        void OnSystemRegister() override {
            REQUIRE(mSignature.none());
            mSignature.set(ecs.GetComponentId<IntComponent>(),true);
            mSignature.set(ecs.GetComponentId<FloatComponent>(),true);
        }
        void Update() override {
            for (const auto &entity : mEntities) {
                auto& componentI = ecs.GetComponent<IntComponent>(entity);
                auto& componentF = ecs.GetComponent<FloatComponent>(entity);
                componentI.i *= 2;
                componentF.f *= .5f;
            }
        }
    };


    // component
    ecs.ResisterComponent<IntComponent>();
    ecs.ResisterComponent<FloatComponent>();
    // system
    auto intInc = ecs.ResisterSystem<IntInc>();
    auto floatInc = ecs.ResisterSystem<FloatInc>();
    auto numMul = ecs.ResisterSystem<NumMul>();
    // ett
    auto ett1 = ecs.CreateEntity();
    auto ett2 = ecs.CreateEntity();
    auto ett3 = ecs.CreateEntity();
    ecs.AddComponent<IntComponent>(ett1, {1});
    ecs.AddComponent<FloatComponent>(ett2, {2.2f});
    ecs.AddComponent<IntComponent>(ett3, {3});
    ecs.AddComponent<FloatComponent>(ett3, {3.3f});

    SECTION("Demo sum first") {
        for (int i = 0; i < 2; ++i) {
            intInc->Update();
            floatInc->Update();
            numMul->Update();
        }

        REQUIRE(ecs.GetComponent<IntComponent>(ett1).i == ((1+1)*2+1)*2);
        REQUIRE(ecs.GetComponent<FloatComponent>(ett2).f == Approx((((2.2f+0.1f)*0.5f)+0.1f)*0.5f));
        REQUIRE(ecs.GetComponent<IntComponent>(ett3).i == ((3+1)*2+1)*2);
        REQUIRE(ecs.GetComponent<FloatComponent>(ett3).f == Approx((((3.3f+0.1f)*0.5f)+0.1f)*0.5f));
    }

    SECTION("Demo mul first") {
        for (int i = 0; i < 2; ++i) {
            numMul->Update();
            intInc->Update();
            floatInc->Update();
        }

        REQUIRE(ecs.GetComponent<IntComponent>(ett1).i == (((1*2)+1)*2)+1);
        REQUIRE(ecs.GetComponent<FloatComponent>(ett2).f == Approx((((2.2f*0.5f)+0.1f)*0.5f)+0.1f));
        REQUIRE(ecs.GetComponent<IntComponent>(ett3).i == (((3*2)+1)*2)+1);
        REQUIRE(ecs.GetComponent<FloatComponent>(ett3).f == Approx((((3.3f*0.5f)+0.1f)*0.5f)+0.1f));
    }
}