# StateMachine 定義受け取り契約

## 概要
`StateMachine` は `MachineDefinition` を直接受け取って構築できます。`Builder` を経由しない定義受け渡しを許容するため、構築時に最小検証を行い、不正な定義の取り込みを防ぎます。

## 検証方針
`StateMachine` コンストラクタは以下を検証します。

- `states` が空ではない
- `root` が有効インデックスで、`composite` であり `root` の親が未設定
- `root` から到達可能
- 各 `composite` が `children` を 1 つ以上持つ
- `composite` の `initialChild` は未指定なら `children` の先頭要素へ正規化される
- `leaf` が `children` を持たないこと
- `leaf` の `initialChild` が未設定であること
- 親子の整合性（`child.parent` が一致）
- 親参照の循環（cycle）なし
- `root` から到達可能な木構造
- すべての transition の `from/to` が有効かつ到達可能
- `outgoing` の参照整合性
  - 各 `outgoing` は有効な transition handle
  - `transition.from` と一致
  - すべての `transitions` がちょうど一度ずつ `outgoing` から参照される

## 注意点
- 検証に失敗した場合、`StateMachine` は無効状態になります。
- `isValid()` は `false` を返します。
- `validationErrors()` で理由を確認できます。
- `formattedValidationErrors()` で改行付き文字列として理由を取得できます。
- 無効な定義を `std::move` 済みで渡した場合、`MachineDefinition` 側は `StateMachine` 側検証で拒否されます。

## 使い方

```cpp
hfsm::StateMachine<MyContext, EventType> machine(std::move(definition));
hfsm::StateMachine<MyContext, EventType>::Instance inst = machine.makeInstance();
hfsm::StateHandle leaf = inst.currentLeaf();

if (!machine.isValid()) {
  for (const auto &error : machine.validationErrors()) {
    // ログ出力など
  }
}
```

## Instance 安全性

- `StateMachine::Instance` は外部から内部状態を直接変更できない不変クラスです。
- 現在状態は `currentLeaf()`、前回状態は `previousLeaf()` で参照します。
- `dispatch` / `tick` は無効なインスタンス状態（未到達状態）を受け取っても例外を投げず `no-op` です。
