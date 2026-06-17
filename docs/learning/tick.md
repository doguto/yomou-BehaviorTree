# tick() の内部処理の解析

ここでは BehaviorTree の実際の実行を担う tick() の内部処理について解析する

## tick() とは

具体的には以下のコードで BehaviorTree は実行されている

```cpp
status = rootNode()->executeTick();
```

このように tick() は　BehaviorTree のルートノードから掘っていき、Running なノードの tick() を叩くことで State の状態遷移を行っている。

## rootNode()

BehaviorTree は内部的に xml を SubTree の vector として保持している。
その vector の先頭要素が大本の Tree になるわけだが、 rootNode() はその先頭要素の持つ nodes vector の先頭要素を返す関数である。

```cpp
TreeNode* Tree::rootNode() const
{
  if(subtrees.empty())
  {
    return nullptr;
  }
  auto& subtree_nodes = subtrees.front()->nodes;
  return subtree_nodes.empty() ? nullptr : subtree_nodes.front().get();
}
```

よって tick() は 先頭の TreeNode から順に tick() を叩いていくことになる。

## executeTick()

```cpp
NodeStatus TreeNode::executeTick()
{
  auto new_status = _p->status;
  PreTickCallback pre_tick;
  PostTickCallback post_tick;
  TickMonitorCallback monitor_tick;
  {
    const std::scoped_lock lk(_p->callback_injection_mutex);
    pre_tick = _p->pre_tick_callback;
    post_tick = _p->post_tick_callback;
    monitor_tick = _p->tick_monitor_callback;
  }

  // a pre-condition may return the new status.
  // In this case it override the actual tick()
  if(auto precond = checkPreConditions())
  {
    new_status = precond.value();
  }
  else
  {
    // injected pre-callback
    bool substituted = false;
    if(pre_tick && !isStatusCompleted(_p->status))
    {
      auto override_status = pre_tick(*this);
      if(isStatusCompleted(override_status))
      {
        // don't execute the actual tick()
        substituted = true;
        new_status = override_status;
      }
    }

    // Call the ACTUAL tick
    if(!substituted)
    {
      using namespace std::chrono;
      // Use atomic_thread_fence to prevent compiler reordering of time measurements.
      // See issue #861 for details.
      const auto t1 = steady_clock::now();
      std::atomic_thread_fence(std::memory_order_seq_cst);
      try
      {
        new_status = tick();
      }
      catch(const NodeExecutionError&)
      {
        // Already wrapped by a child node, re-throw as-is to preserve original info
        throw;
      }
      catch(const std::exception& ex)
      {
        // Wrap the exception with this node's context
        throw NodeExecutionError({ name(), fullPath(), registrationName() }, ex.what());
      }
      std::atomic_thread_fence(std::memory_order_seq_cst);
      const auto t2 = steady_clock::now();
      if(monitor_tick)
      {
        monitor_tick(*this, new_status, duration_cast<microseconds>(t2 - t1));
      }
    }
  }

  // injected post callback
  if(isStatusCompleted(new_status))
  {
    checkPostConditions(new_status);
  }

  if(post_tick)
  {
    auto override_status = post_tick(*this, new_status);
    if(isStatusCompleted(override_status))
    {
      new_status = override_status;
    }
  }

  // preserve the IDLE state if skipped, but communicate SKIPPED to parent
  if(new_status != NodeStatus::SKIPPED)
  {
    setStatus(new_status);
  }
  return new_status;
}
```

### _p とは？

tree_node.cpp に定義されている PImpl 構造体のポインタ

```cpp
struct TreeNode::PImpl
{
  PImpl(std::string name, NodeConfig config)
    : name(std::move(name)), config(std::move(config))
  {}

  const std::string name;

  NodeStatus status = NodeStatus::IDLE;

  std::condition_variable state_condition_variable;

  mutable std::mutex state_mutex;

  StatusChangeSignal state_change_signal;

  NodeConfig config;

  std::string registration_ID;

  PreTickCallback pre_tick_callback;
  PostTickCallback post_tick_callback;
  TickMonitorCallback tick_monitor_callback;

  std::mutex callback_injection_mutex;

  std::shared_ptr<WakeUpSignal> wake_up;

  std::array<ScriptFunction, size_t(PreCond::COUNT_)> pre_parsed;
  std::array<ScriptFunction, size_t(PostCond::COUNT_)> post_parsed;
};
```

private なメンバ変数を全てこの構造体に定義している

.hpp には以下のように構造体を使用することだけが定義されており、実装の詳細が隠蔽されている

```cpp
struct PImpl;
std::unique_ptr<PImpl> _p;
```

これにより .cpp の入れ替えによる実装の変更があっても .hpp をインクルードしているコードに影響が出ないようになっている。

また .hpp が変更しないということは tree_node.hpp を変更しても include しているコードに再コンパイルが発生しないということを意味しており、コンパイル時間の短縮にも寄与している。

ちなみに、 unique_ptr で持っているから、PImpl のメンバ変数の変更があっても TreeNode のサイズは変わらなくコンパイル時間の短縮が出来ているが、ポインタではなく実体で持ってしまうと TreeNode のサイズが変わってしまい、TreeNode を使用しているコード全てに再コンパイルが発生してしまう。

### XxxCanllback

以下のように Callback が定義されている

```cpp
PreTickCallback pre_tick;
PostTickCallback post_tick;
TickMonitorCallback monitor_tick;
```

これは tick() の実行結果をこれで上書きすることができるようになっている。
デバッグツールで指定の tick() で一旦停止する等したい場合に使用する

以下の関数群で設定できる

```cpp
void TreeNode::setPreTickFunction(PreTickCallback callback)
{
  const std::unique_lock lk(_p->callback_injection_mutex);
  _p->pre_tick_callback = std::move(callback);
}

void TreeNode::setPostTickFunction(PostTickCallback callback)
{
  const std::unique_lock lk(_p->callback_injection_mutex);
  _p->post_tick_callback = std::move(callback);
}

void TreeNode::setTickMonitorCallback(TickMonitorCallback callback)
{
  const std::unique_lock lk(_p->callback_injection_mutex);
  _p->tick_monitor_callback = std::move(callback);
}
```

これらの関数群との衝突を避けるため、 executeTick() 内では mutex で排他制御を行っている。

```cpp
{
  const std::scoped_lock lk(_p->callback_injection_mutex);
  pre_tick = _p->pre_tick_callback;
  post_tick = _p->post_tick_callback;
  monitor_tick = _p->tick_monitor_callback;
}
```

### checkPreConditions()

BehaviorTree の各ノードは FAILURE_IF, SUCCESS_IF, SKIP_IF, WHILE_TRUE の4種類の PreCondition を持つことができる。

```xml
<MyAction _skipIf="condition == false" _failureIf="error == true" />
```

これが設定されている場合にその事前条件を確認し、条件が満たされている場合は tick() を実行せずにその条件に対応する NodeStatus を返すのが checkPreConditions() の役割

Parse関連の処理が見えるが、一旦本筋とずれるので割愛する

### tick() の実行

先述した pre_tick 等が無ければ、ここで実際に tick() が実行される

カスタムノードなら自分で実装した tick() が呼ばれ、 Condition ノード等の子を持つノードでは子ノードに対して再度 executeTick() が呼ばれることになる。

以下は SubTree の tick() の実装例

```cpp
BT::NodeStatus BT::SubTreeNode::tick()
{
  const NodeStatus prev_status = status();
  if(prev_status == NodeStatus::IDLE)
  {
    setStatus(NodeStatus::RUNNING);
  }
  const NodeStatus child_status = child_node_->executeTick();
  if(isStatusCompleted(child_status))
  {
    resetChild();
  }

  return child_status;
}
```

## まとめ

このように、 BehaviorTree は rootNode() から tick() で掘っていき、再帰的に executeTick() を呼び出すことで、各ノードの状態遷移を行っている。

実際の遷移等は SequenceNode などの子ノードを持つノードの tick() の実装に依存する。