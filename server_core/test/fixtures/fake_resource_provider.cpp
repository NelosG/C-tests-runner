// Fake resource provider .so used by ResourceManager unit tests.
// Implements the full ResourceProvider contract with deterministic behaviour
// driven by the input config:
//   - config.reject = true  -> validate_config returns false with message
//   - config.throw_start = true -> start() throws
// resolve() returns a path constructed from the descriptor "id" field.

#include <resource_context.h>
#include <resource_provider.h>

#include <stdexcept>
#include <string>

namespace {

// Treat null / non-object configs as empty objects so `value()` is always safe.
static bool flag(const nlohmann::json& j, const std::string& key) {
    return j.is_object() && j.value(key, false);
}

class FakeProvider : public ResourceProvider {
    public:
        explicit FakeProvider(nlohmann::json cfg) : config_(std::move(cfg)) {}

        std::string name() const override { return "fake"; }

        std::filesystem::path resolve(const nlohmann::json& descriptor) override {
            // Echo back the "id" field as a path so tests can assert routing.
            std::string id = descriptor.is_object()
                ? descriptor.value("id", std::string("default"))
                : std::string("default");
            return std::filesystem::path("/tmp/fake/") / id;
        }

        bool validate_config(const nlohmann::json& cfg, std::string& error) override {
            if(flag(cfg, "reject")) {
                error = "rejected-by-fake";
                return false;
            }
            return true;
        }

        void start() override {
            if(flag(config_, "throw_start")) {
                throw std::runtime_error("fake-provider-start-explode");
            }
        }

        void stop() override {}

    private:
        nlohmann::json config_;
};

} // namespace

extern "C" const char* provider_name() {
    return "fake";
}

extern "C" ResourceProvider* create_provider(const ResourceContext* ctx) {
    if(!ctx) return nullptr;
    if(flag(ctx->config, "ctor_null")) return nullptr;
    return new FakeProvider(ctx->config);
}

extern "C" void destroy_provider(ResourceProvider* p) {
    delete p;
}
