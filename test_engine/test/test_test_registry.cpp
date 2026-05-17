// Unit tests for TestRegistry - the singleton + per-thread TLS registry that
// REGISTER_TEST() targets when plugin DLLs are loaded.

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <test_registry.h>
#include <test_scenario_extension.h>
#include <thread>


namespace {

    /// Minimal TestScenarioExtension stub - name() drives registry deduplication.
    class StubScenario : public TestScenarioExtension {
        public:
            explicit StubScenario(std::string name) : name_(std::move(name)) {}
            std::vector<Test> get_tests() const override { return {}; }
            std::string name() const override { return name_; }

        private:
            std::string name_;
    };

    std::unique_ptr<StubScenario> make(const std::string& name) {
        return std::make_unique<StubScenario>(name);
    }

} // namespace

// -----------------------------------------------------------------------------
// Behaviour on a fresh (or cleared) registry
// -----------------------------------------------------------------------------

class TestRegistryTest : public ::testing::Test {
    protected:
        TestRegistry registry_;

        void SetUp() override {
            // Route REGISTER_TEST() and TestRegistry::instance() to the fixture's
            // local registry so this suite never touches the process-wide singleton.
            TestRegistry::set_active_instance(&registry_);
        }

        void TearDown() override {
            TestRegistry::clear_active_instance();
        }
};

TEST_F(TestRegistryTest, fresh_registry_is_empty) {
    EXPECT_EQ(registry_.size(), 0u);
    EXPECT_TRUE(registry_.all().empty());
}

TEST_F(TestRegistryTest, register_test_accepts_valid_scenario) {
    EXPECT_TRUE(registry_.register_test(make("Alpha")));
    EXPECT_EQ(registry_.size(), 1u);
    ASSERT_EQ(registry_.all().size(), 1u);
    EXPECT_EQ(registry_.all().front()->name(), "Alpha");
}

TEST_F(TestRegistryTest, register_test_rejects_nullptr) {
    EXPECT_FALSE(registry_.register_test(nullptr));
    EXPECT_EQ(registry_.size(), 0u);
}

TEST_F(TestRegistryTest, register_test_rejects_duplicate_name) {
    EXPECT_TRUE(registry_.register_test(make("Same")));
    EXPECT_FALSE(registry_.register_test(make("Same")));
    EXPECT_EQ(registry_.size(), 1u);
}

TEST_F(TestRegistryTest, register_test_distinct_names_coexist) {
    EXPECT_TRUE(registry_.register_test(make("A")));
    EXPECT_TRUE(registry_.register_test(make("B")));
    EXPECT_TRUE(registry_.register_test(make("C")));
    EXPECT_EQ(registry_.size(), 3u);
}

TEST_F(TestRegistryTest, clear_removes_all_entries) {
    registry_.register_test(make("X"));
    registry_.register_test(make("Y"));
    ASSERT_EQ(registry_.size(), 2u);
    registry_.clear();
    EXPECT_EQ(registry_.size(), 0u);
    EXPECT_TRUE(registry_.all().empty());
}

TEST_F(TestRegistryTest, registration_order_is_preserved) {
    registry_.register_test(make("first"));
    registry_.register_test(make("second"));
    registry_.register_test(make("third"));
    const auto& all = registry_.all();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all[0]->name(), "first");
    EXPECT_EQ(all[1]->name(), "second");
    EXPECT_EQ(all[2]->name(), "third");
}

// -----------------------------------------------------------------------------
// Thread-local active instance override
// -----------------------------------------------------------------------------

TEST(TestRegistryTLSTest, instance_returns_global_without_override) {
    TestRegistry::clear_active_instance();
    TestRegistry& a = TestRegistry::instance();
    TestRegistry& b = TestRegistry::instance();
    EXPECT_EQ(&a, &b) << "Global singleton should be stable across calls";
}

TEST(TestRegistryTLSTest, instance_routes_to_active_when_set) {
    TestRegistry local;
    TestRegistry::set_active_instance(&local);
    EXPECT_EQ(&TestRegistry::instance(), &local);
    TestRegistry::clear_active_instance();
    EXPECT_NE(&TestRegistry::instance(), &local);
}

TEST(TestRegistryTLSTest, active_override_does_not_leak_to_other_threads) {
    TestRegistry main_local;
    TestRegistry::set_active_instance(&main_local);

    std::atomic<bool> other_sees_main{false};
    std::thread worker(
        [&] {
            // Worker thread sets nothing - its TLS slot is null, so instance()
            // must return the process-global singleton, NOT main_local.
            other_sees_main.store(&TestRegistry::instance() == &main_local);
        }
    );
    worker.join();
    EXPECT_FALSE(other_sees_main.load()) << "TLS must not leak across threads";

    TestRegistry::clear_active_instance();
}

TEST(TestRegistryTLSTest, two_local_instances_are_independent) {
    TestRegistry r1;
    TestRegistry r2;

    TestRegistry::set_active_instance(&r1);
    EXPECT_TRUE(TestRegistry::instance().register_test(make("only-in-r1")));

    TestRegistry::set_active_instance(&r2);
    EXPECT_TRUE(TestRegistry::instance().register_test(make("only-in-r2")));
    // Duplicate name across registries is fine - they are independent maps.
    EXPECT_TRUE(TestRegistry::instance().register_test(make("only-in-r1")));

    TestRegistry::clear_active_instance();

    EXPECT_EQ(r1.size(), 1u);
    EXPECT_EQ(r2.size(), 2u);
}
