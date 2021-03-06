/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#pragma once

#include <string>
#include <thread>  // NOLINT
#include <utility>

#include "grpc++/grpc++.h"
#include "paddle/fluid/framework/executor.h"
#include "paddle/fluid/framework/lod_tensor.h"
#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/framework/scope.h"
#include "paddle/fluid/framework/selected_rows.h"
#include "paddle/fluid/framework/var_type.h"
#include "paddle/fluid/operators/detail/grpc_service.h"
#include "paddle/fluid/operators/detail/send_recv.grpc.pb.h"
#include "paddle/fluid/operators/detail/send_recv.pb.h"
#include "paddle/fluid/operators/detail/sendrecvop_utils.h"
#include "paddle/fluid/operators/detail/simple_block_queue.h"

namespace paddle {
namespace operators {
namespace detail {

typedef std::pair<std::string, std::shared_ptr<VariableResponse>>
    ReceivedMessage;
typedef SimpleBlockQueue<ReceivedMessage> ReceivedQueue;

typedef std::pair<std::string, sendrecv::VariableMessage> MessageWithName;
class RequestBase;

class AsyncGRPCServer final {
 public:
  explicit AsyncGRPCServer(const std::string &address) : address_(address) {}

  void RunSyncUpdate();

  // functions to sync server barrier status.
  void WaitCond(int cond);
  void SetCond(int cond);
  void WaitClientGet(int count);

  void SetScope(framework::Scope *scope) { scope_ = scope; }

  void SetDevCtx(const platform::DeviceContext *dev_ctx) { dev_ctx_ = dev_ctx; }

  void SetProgram(framework::ProgramDesc *program) { program_ = program; }

  void SetPrefetchBlkdId(int blkid) { prefetch_blk_id_ = blkid; }

  void SetExecutor(framework::Executor *executor) { executor_ = executor; }

  int GetSelectedPort() { return selected_port_; }

  const ReceivedMessage Get() { return this->var_recv_queue_.Pop(); }

  void Push(const std::string &msg_name) {
    this->var_recv_queue_.Push(std::make_pair(msg_name, nullptr));
  }

  void ShutDown();

 protected:
  void HandleRequest(::grpc::ServerCompletionQueue *cq,
                     const std::string &cq_name,
                     std::function<void()> TryToRegisterNewOne);
  void TryToRegisterNewSendOne();
  void TryToRegisterNewGetOne();
  void TryToRegisterNewPrefetchOne();
  void ShutdownQueue();

 private:
  std::mutex cq_mutex_;
  volatile bool is_shut_down_ = false;
  std::unique_ptr<::grpc::ServerCompletionQueue> cq_send_;
  std::unique_ptr<::grpc::ServerCompletionQueue> cq_get_;
  std::unique_ptr<::grpc::ServerCompletionQueue> cq_prefetch_;

  GrpcService::AsyncService service_;
  std::unique_ptr<::grpc::Server> server_;

  std::string address_;
  framework::Scope *scope_;
  const platform::DeviceContext *dev_ctx_;

  // received variable from RPC, operators fetch variable from this queue.
  SimpleBlockQueue<MessageWithName> var_get_queue_;
  // client send variable to this queue.
  ReceivedQueue var_recv_queue_;

  // condition of the sub program
  std::mutex barrier_mutex_;
  mutable int barrier_cond_step_;
  std::condition_variable barrier_condition_;

  std::unique_ptr<std::thread> t_send_;
  std::unique_ptr<std::thread> t_get_;
  std::unique_ptr<std::thread> t_prefetch_;

  int prefetch_blk_id_;
  framework::ProgramDesc *program_;
  framework::Executor *executor_;
  int selected_port_;
};

};  // namespace detail
};  // namespace operators
};  // namespace paddle
