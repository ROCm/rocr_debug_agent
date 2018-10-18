#include "util.h"
#include "vector_add_debug_trap.h"
#include "vector_add_normal.h"
#include "vector_add_memory_fault.h"

static void PrintTestInfo(const char *header);
static void RunVectorAddDebugTrapTest();
static void RunVectorAddNormalTest();
static void RunVectorAddMemoryFaultTest();
std::string isaName;

int main(int argc, char *argv[]) {
  PrintTestInfo("Debug agent tests start");
  std::vector<unsigned int> run_test_list;
  if (argc == 1) {
    run_test_list.push_back(0);
    run_test_list.push_back(1);
    run_test_list.push_back(2);
  }
  else {
    int i = 1;
    while(i <= argc - 1) {
      unsigned int test_id = static_cast<int>(*argv[i]) - '0';
      run_test_list.push_back(test_id);
      i ++;
    }
  }

  for (unsigned int i = 0 ; i < run_test_list.size(); i++) {
    switch(run_test_list.at(i)) {
      case 0:
        RunVectorAddNormalTest();
        break;
      case 1:
        RunVectorAddDebugTrapTest();
        break;
      case 2:
        RunVectorAddMemoryFaultTest();
        break;
      default:
        std::cout << "  *** Invalid Test ID ***" << std::endl;
        break;
    }
  }

  PrintTestInfo("Debug agent test finished");
  std::cout << TEST_SEPARATOR << std::endl;
}

static void PrintTestInfo(const char *info) {
  std::cout << "  *** Debug Agent Test: " << info << " ***" << std::endl;
}

static void RunVectorAddNormalTest()
{
  hsa_status_t err;

  // initiale hsa
  err = hsa_init();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddNormal start");

  // find all cpu agents
  std::vector<hsa_agent_t> cpus;
  err = hsa_iterate_agents(IterateCPUAgents, &cpus);
  assert(err == HSA_STATUS_SUCCESS);

  // find all gpu agents
  std::vector<hsa_agent_t> gpus;
  err = hsa_iterate_agents(IterateGPUAgents, &gpus);
  assert(err == HSA_STATUS_SUCCESS);

  if (gpus.size() == 0){
    std::cout << "No supported GPU found, exit test." << std::endl;
  }
  else {
    for (unsigned int i = 0 ; i< gpus.size(); ++i) {
      VectorAddNormalTest(cpus[0], gpus[i]);
    }
  }

  // Shut down hsa
  err = hsa_shut_down();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddNormal end");
}

static void RunVectorAddDebugTrapTest()
{
  hsa_status_t err;

  // initiale hsa
  err = hsa_init();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddDebugTrapTest start");

  // find all cpu agents
  std::vector<hsa_agent_t> cpus;
  err = hsa_iterate_agents(IterateCPUAgents, &cpus);
  assert(err == HSA_STATUS_SUCCESS);

  // find all gpu agents
  std::vector<hsa_agent_t> gpus;
  err = hsa_iterate_agents(IterateGPUAgents, &gpus);
  assert(err == HSA_STATUS_SUCCESS);

  if (gpus.size() == 0){
    std::cout << "No supported GPU found, exit test." << std::endl;
  }
  else {
    for (unsigned int i = 0 ; i< gpus.size(); ++i) {
      VectorAddDebugTrapTest(cpus[0], gpus[i]);
    }
  }

  // Shut down hsa
  err = hsa_shut_down();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddDebugTrapTest end");
}

static void RunVectorAddMemoryFaultTest()
{
  hsa_status_t err;

  // initiale hsa
  err = hsa_init();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddMemoryFaultTest start");

  // find all cpu agents
  std::vector<hsa_agent_t> cpus;
  err = hsa_iterate_agents(IterateCPUAgents, &cpus);
  assert(err == HSA_STATUS_SUCCESS);

  // find all gpu agents
  std::vector<hsa_agent_t> gpus;
  err = hsa_iterate_agents(IterateGPUAgents, &gpus);
  assert(err == HSA_STATUS_SUCCESS);

  if (gpus.size() == 0){
    std::cout << "No supported GPU found, exit test." << std::endl;
  }
  else {
    for (unsigned int i = 0 ; i< gpus.size(); ++i) {
      VectorAddMemoryFaultTest(cpus[0], gpus[i]);
    }
  }

  // Shut down hsa
  err = hsa_shut_down();
  assert(err == HSA_STATUS_SUCCESS);

  PrintTestInfo("VectorAddMemoryFaultTest end");
}
