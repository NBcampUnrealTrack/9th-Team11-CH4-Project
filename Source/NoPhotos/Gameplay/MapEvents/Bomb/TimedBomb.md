# 임시 폭탄: NPTimedBomb

`ANPTimedBomb`는 생성 후 기본 **5초**에 서버에서 한 번 폭발하는 액터입니다. 맵 이벤트 폴더에 두되 `ANPBaseRelic`을 상속하여 산타 선물의 유물 후보로도 사용할 수 있게 했습니다. 별도 맵 이벤트 등록이나 자동 스폰은 추가하지 않았습니다.

## 동작

- 서버 BeginPlay부터 Fuse Duration을 계산합니다. 잡기/놓기/착지로 시간을 다시 시작하지 않습니다. 레벨에 미리 배치하면 플레이 시작부터 계산합니다.
- 기본 외형은 엔진 기본 구체(스케일 0.4)입니다. 기존 유물 잡기와 물리·이동 복제를 사용하며, 전시 상태가 아닌 물리 활성 상태로 시작합니다.
- 폭발 시 현재 위치를 중심으로 반경 500cm 내 `NPStablePhysicsPawn` 계열 중 초기화된 `NPAbilitySystemComponent`를 가진 대상을 한 번씩 처리합니다. 소유자/폭탄을 잡고 있던 플레이어도 범위 내라면 포함합니다.
- 기존 `NPKnockbackGameplayEffect`에 `Effect.Knockback` 태그와 `Data.Knockback.Magnitude`를 전달합니다. 기존 ASC가 캐릭터의 `AddExternalVelocityChange` 경로로 넉백을 동기화합니다. 별도 GE BP나 태그 생성은 필요 없습니다.
- 속도는 수평 바깥 방향 1000cm/s + 위쪽 600cm/s입니다. 거리 감쇠는 없으며 중심과 XY가 같은 대상은 위쪽으로만 밀어냅니다. 기본적으로 Visibility를 막는 엄폐물 뒤의 대상은 제외합니다.
- **체력 피해, 유물/유리 파괴, 일반 물리 소품의 방사 충격, 연쇄 폭발은 추가하지 않았습니다.** 기존 GAS가 없는 캐릭터는 넉백 대상이 아닙니다.
- 폭발 즉시 잡기를 해제하고 메시·충돌·물리를 끕니다. 서버가 넉백을 확정하고 Reliable Multicast로 폭발 위치와 연출을 전달합니다. 액터는 2초 후 제거됩니다.
- 중도 접속자는 복제된 종료 시각으로 남은 시간을 확인할 수 있습니다. 이미 폭발한 폭탄은 숨긴 상태로 처리하며 이전 폭발 연출을 다시 재생하지 않습니다.
- 폭탄을 외부에서 제거하면 타이머를 정리하며 추가 폭발은 없습니다. 산타 이벤트 종료와는 무관합니다. 반납은 타이머를 취소하지 않으므로 현재 임시 폭탄을 반납용 유물로 사용할 때는 주의하세요.

## 사용자 에디터 설정

빌드와 BP/DA 수정은 사용자가 진행합니다.

1. 부모 `NPTimedBomb`로 `BP_TimedBomb`를 만듭니다. 기존 `RelicMesh`의 메시/머티리얼/스케일을 바꿀 수 있습니다. 교체하는 메시에는 Simple Collision과 Query And Physics가 필요합니다.
2. **Timed Bomb → Fuse Duration = 5**를 유지합니다. BP의 Initial Life Span은 0으로 두고 별도 Delay/Destroy/폭발 타이머는 연결하지 않습니다.
3. **Explosion Radius**, **Horizontal Knockback Strength**, **Upward Knockback Strength**로 범위와 힘을 조절합니다. **Require Line Of Sight**를 끄면 벽 너머에도 적용합니다.
4. 실제 연출은 **Explosion System / Explosion Sound**에 지정합니다. 기본은 미지정이며 디버그 드로잉이 활성화된 빌드에서 주황색 범위 구체를 0.75초 표시합니다. Shipping용 연출은 Niagara/사운드를 지정하세요. 원샷 Niagara를 사용해야 잔여 이펙트가 남지 않습니다.
5. 추가 연출은 **On Bomb Exploded**에서 구현합니다. 연출 전용이며 여기서 넉백/유물 Spawn을 중복 실행하지 마세요. 나이아가라는 폭탄에 부착하지 않고 독립 위치에 생성합니다.
6. 부모의 BeginPlay를 호출하도록 유지하고 Replicates / Replicate Movement를 켭니다. GAS용 `ExplosionAbilitySystem`과 잡기 컴포넌트는 C++에서 생성하므로 BP에 중복 추가하지 않습니다.

산타 선물에 넣으려면 `DA_Santa → Santa Event → Gifts → Relic Classes`에 `BP_TimedBomb`를 추가하세요. 선물 상자를 잡은 시점이 아니라 **개봉 연출 완료 후 폭탄이 실제 생성되는 시점부터 5초**입니다. 기존 유물과 함께 등록하면 동일 확률로 추첨됩니다. 이번 작업에서 DA 목록을 수정하지는 않았습니다.

별도 테스트는 레벨에 직접 배치하거나 서버 `SpawnActor`로 생성하면 됩니다. UI용 남은 시간은 `GetRemainingFuseTime`, 폭발 여부는 `HasExploded`로 확인합니다.

## 빌드 후 확인 항목

빌드/PIE/자동화 테스트는 아직 실행하지 않았습니다. 계산 테스트 소스는 `NoPhotos.MapEvents.Bomb.BlastVelocity`에 추가했습니다. 아래는 실제 플레이에서 별도 검증해야 합니다.

1. 생성 후 5초 전에는 폭발하지 않고 5초에 한 번 폭발하는지. 잡거나 놓아도 시간이 리셋되지 않는지.
2. 반경 안/밖, 벽 뒤, 중심 바로 위에서 넉백 방향과 강도가 맞는지. 여러 신체 컴포넌트로 중복 넉백이 발생하지 않는지.
3. Listen Server 호스트/원격 클라이언트 및 Dedicated Server에서 폭발·잡기 해제·넉백이 일치하는지.
4. 폭발 직전 잡기, 폭발 전 중도 접속, 폭발 직후 중도 접속, 폭탄 강제 제거 시 이중 폭발이나 잔여 잡기가 없는지.
5. 산타 선물에서 생성한 폭탄도 생성 5초 후 폭발하는지. 산타가 먼저 떠나도 폭탄은 정상 동작하는지.
