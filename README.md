# hfsm

`DokuEngine` の実装方針をベースにした、ヘッダオンリーの階層有限状態機械ライブラリです。  
`include/hfsm/hfsm.hpp` を中核に、`StateMachineBuilder` で遷移グラフを組み立て、`StateMachine` で実行します。

## ディレクトリ構成

- `include/hfsm/hfsm.hpp`: コア実装
- `samples/basic.cpp`: 基本サンプル（階層状態、基本遷移）
- `samples/guard.cpp`: ガード付き遷移サンプル（成立/不成立の見え方）
- `tests/test_hfsm.cpp`: 簡易テスト
- `docs/state-machine-definition-contract.md`: StateMachine の直接定義受け取り契約
- `CMakeLists.txt`: ビルド定義

## 主要 API

- `StateMachineBuilder<ContextType, EventType = EventId>`
  - `addLeaf(name, callbacks)`
  - `addComposite(name, callbacks)`
  - `setInitial(parent, child)`
  - `addChild(parent, child)`
  - `addTransition(from, event, to, priority = 0)`
  - `setGuard(transition, guard)`
  - `build(root) -> BuildResult` (`MachineDefinition`, `errors`, `ok()`)
- `StateMachine<ContextType, EventType = EventId>`
  - `makeInstance()`
  - `dispatch(instance, event, context)`
  - `tick(instance, event, context)`
  - `StateMachine::Instance::currentLeaf()` / `previousLeaf()` / `stateName(handle)`
  - インスタンス状態の参照は `currentLeaf()` / `previousLeaf()` を使います。
  - `validationErrors()`（`StateMachine` 直接構築時の検証失敗原因）
### `makeInstance()` / `dispatch()` / `tick()` の仕様

- `makeInstance()` は `root` から `initialChild` を辿って leaf まで降りるだけで、`onEntry` は呼びません。
- `dispatch(instance, event, context)` は現在 leaf から parent 方向に遷移候補を検索します。
- `dispatch` は優先順位順（高い priority 優先、同 priority は追加順）で最初に成立した遷移を採用します。
- `guard` がない遷移は常時成立です。
  - `tick(instance, event, context)` は現在 leaf の `onPerform` のみを呼びます（enter/exit は呼びません）。
  - 親状態への遷移を選んだ場合、`findTransition` は葉から上位を辿って評価されます。
  - `instance` は `StateMachine::Instance` で、現在状態は `currentLeaf()`、直前状態は `previousLeaf()` で参照します。
    既定状態 `Instance{}` は未開始状態で、`dispatch` / `tick` は no-op です。
  - `build()` は `hfsm::BuildResult` を返し、`ok()` が true の場合のみ `definition` が有効です。
  - `build()` は妥当性チェック結果を `errors` に格納するため、必ず `ok()` を確認してください。

## 外部導入（find_package）

配布済みパッケージを使う場合は次を追加します。

```cmake
find_package(hfsm CONFIG REQUIRED)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE hfsm::hfsm)
target_compile_features(app PRIVATE cxx_std_20)
```

`tests/package_consumer/main.cpp` は最小導入例として同等の検証コードを置いています。

## CMake（ライブラリターゲット）

`hfsm` はヘッダオンリーとして `hfsm::hfsm` のインターフェースターゲットを提供します。

```powershell
cmake -S . -B build -DHFSM_BUILD_SAMPLES=ON -DHFSM_BUILD_TESTS=ON
cmake --build build
```

`CMakeLists.txt` の要点（初期はトップレベルのみサンプル/テストを有効化）:

```cmake
add_library(hfsm INTERFACE)
add_library(hfsm::hfsm ALIAS hfsm)

target_compile_features(hfsm INTERFACE cxx_std_20)
target_include_directories(hfsm INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
```

## ビルド（CMake）

```powershell
cmake -S . -B build
cmake --build build
```

生成物:

- `build\hfsm-sample-basic.exe`（基本サンプル）
- `build\hfsm-sample-guard.exe`（ガードサンプル）
- `build\hfsm-tests.exe`（テスト、`ctest` で実行）

## テスト

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

テスト観点（`tests/test_hfsm.cpp`）:

- Build 失敗ケース: ルートが composite でない構成が検出されること
- ガード成立 / 不成立: `context` 条件で遷移が切り替わること
- 遷移優先度: 同一イベント時に priority の高い遷移が選ばれること
- direct `MachineDefinition` の危険系検証:
  - 無効な parent handle
  - leaf が children を持つ構成
  - leaf に `initialChild` がある構成
  - `outgoing` が transition を参照しない / 二重参照
  - initialChild 未設定 composite が normalize されること
  - `Instance{}` の no-op

`stateIndex(handle)` / `transitionIndex(handle)` は、state/transition 配列添字が必要な高度な用途向けの API です。`invalid handle` に対して呼ばないでください。

## BuildResult の最小サンプル

```cpp
enum class MyEvent {
  kPowerOn,
  kPowerOff,
};

hfsm::StateMachineBuilder<MyContext, MyEvent> builder;
// ... 状態と遷移を追加 ...
const auto buildResult = builder.build(root);
if (!buildResult.ok()) {
  std::cerr << buildResult.formattedErrors('\n') << '\n';
  return 1;
}

hfsm::StateMachine<MyContext, MyEvent> machine(std::move(*buildResult.definition));
```

## 補足

- 実行例:

```powershell
build\hfsm-sample-basic
build\hfsm-sample-guard
```
