#include "test_runner_service.h"

TestRunnerService::TestRunnerService(
    BuildService::BuildConfig config,
    SandboxLauncher::Config sandbox_config,
    CpuIsolator::Config cpu_config,
    ResourceManager& resource_manager
)
    : build_service_(std::move(config)),
      sandbox_(std::move(sandbox_config)),
      cpu_isolator_(std::move(cpu_config)),
      test_executor_(sandbox_, cpu_isolator_),
      queue_(
          std::make_unique<JobQueue>(
              JobQueue::JobExecutor{},
              build_service_.config().correctness_workers
          )
      ),
      pipeline_(
          build_service_,
          test_executor_,
          resource_manager,
          *queue_
      ) {
    // Wire the queue's executor to Pipeline::execute now that both exist.
    queue_->set_executor(
        [this](
        const nlohmann::json& req,
        std::function<void(job_status)> updater,
        progress::callback on_progress
    ) {
            return pipeline_.execute(req, node_id_, std::move(updater), std::move(on_progress));
        }
    );
}

std::string TestRunnerService::submit(
    nlohmann::json request,
    CompletionCallback on_complete,
    progress::callback on_progress
) {
    return queue_->submit(std::move(request), std::move(on_complete), std::move(on_progress));
}

JobQueue::JobInfo TestRunnerService::get_job_info(const std::string& job_id) const {
    return queue_->get_job_info(job_id);
}

nlohmann::json TestRunnerService::get_queue_status() const {
    return queue_->get_status();
}

bool TestRunnerService::cancel(const std::string& job_id) {
    return queue_->cancel(job_id);
}

void TestRunnerService::set_max_correctness_workers(int n) {
    queue_->resize_correctness_pool(n);
}

void TestRunnerService::set_job_retention_seconds(int sec) {
    queue_->set_job_retention_seconds(sec);
}

int TestRunnerService::job_retention_seconds() const {
    return queue_->job_retention_seconds();
}
