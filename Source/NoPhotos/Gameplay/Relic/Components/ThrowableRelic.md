# 던지는 유물 컴포넌트

## 블루프린트 설정

1. `ANPBaseRelic` 자식 유물 BP를 엽니다.
2. `NPThrowableRelicComponent`를 추가합니다.
3. 같은 유물에는 `NPSwingableRelicComponent`나 다른 `NPUsableRelicComponent`를 함께 달지 않습니다. 현재 그랩 시스템은 첫 번째 Usable 컴포넌트의 Ability만 지급합니다.
4. 유물 루트 메시의 Collision Preset은 물리 충돌을 지원해야 하며, 메시 에셋에는 Simple Collision이 있어야 합니다.
5. `Relic Ability > Throw Settings`를 조절합니다.

별도 Input Action이나 Ability BP는 필요하지 않습니다. 기존 `RelicUseAction`에 연결된 상호작용 키를 누르면 네이티브 `NPThrowableRelicUseAbility`가 실행됩니다.

## 기본 동작

- 오른손으로 해당 유물 하나만 잡은 상태에서만 사용할 수 있습니다.
- 서버가 실제로 잡고 있는 유물인지 다시 확인합니다.
- 사용 시 모든 그랩을 먼저 해제하고 유물 물리를 활성화합니다.
- 카메라가 바라보는 3차원 방향으로 `Forward Speed`를 적용하고, 월드 위쪽으로 `Upward Speed`를 더합니다.
- `Inherit Thrower Velocity`가 켜져 있으면 플레이어 이동 속도를 한 번 더합니다.
- `Local Spin Axis`를 유물의 월드 방향으로 변환하여 `Spin Speed` 각속도를 적용합니다. 기본 로컬 Y축은 무기를 앞뒤로 회전시키는 값입니다. 메시 축이 다르면 X/Y/Z를 바꿉니다.
- 기본 0.2초 동안 Pawn 채널을 무시한 뒤 이전 충돌 응답을 복구합니다. 이 시간에는 던진 사람뿐 아니라 다른 Pawn도 통과할 수 있으므로 가까운 대상 타격이 필요하면 값을 줄이거나 0으로 둡니다.
- 투척된 실제 유물 액터의 Replicate Movement를 그대로 사용합니다. 장식용 투사체를 따로 생성하지 않습니다.

기본값은 전진 1800cm/s, 위 300cm/s, 회전 1440deg/s, 재사용 대기 0.5초입니다. 투척 자체에는 대미지나 GAS 넉백을 넣지 않았으며 기존 물리 충돌만 발생합니다.

## 확인 사항

빌드 후 유물 BP에 컴포넌트를 추가하고 다음을 확인합니다.

- 혼자 들었을 때만 기존 Relic Use 키로 투척되는지
- 손 그랩이 해제되고 유물이 전진하면서 회전하는지
- 달리면서 던질 때 플레이어 속도가 자연스럽게 더해지는지
- 약 0.2초 뒤 Pawn 충돌이 원래 설정으로 돌아오는지
- Listen Server와 Client 양쪽에서 같은 유물 위치·회전이 보이는지
- Simple Collision이 없는 메시에서 경고와 함께 안전하게 실패하는지

자동화 테스트 소스 `NoPhotos.Relic.Throwable.Velocity`, `NoPhotos.Relic.Throwable.Spin`도 추가했습니다. 요청에 따라 빌드와 테스트 실행은 하지 않았습니다.

