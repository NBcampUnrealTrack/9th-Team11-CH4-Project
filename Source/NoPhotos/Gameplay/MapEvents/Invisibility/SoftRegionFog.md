# 각진 안개막을 부드러운 영역 안개로 변경하기

## 변경 이유와 범위

기존 `RegionFogOpacity → Opacity` 연결은 Cube 표면을 균일하게 칠하므로 하늘을 배경으로 보면 직선과 모서리가 드러납니다. DepthFade는 지면/불투명 물체와의 교차부를 완화할 뿐, 허공의 박스 모서리를 없애지는 않습니다.

새 방식은 Cube를 계산 범위로만 사용하고, 시선이 영역을 통과한 구간에서 안개 밀도를 32회 샘플링합니다. 가장자리에서는 밀도가 0으로 감소하고, 모서리는 둥글게 감쇠합니다. 내부 농도도 천천히 변화합니다. Cube 표면의 일정한 알파를 그대로 보여주지 않습니다.

**SpawnVolume, 서버 진입/이탈 판정, 개인 시야 후처리, UI/사진 코드는 변경하지 않았습니다.** 부드러운 표시와 달리 실제 진입 판정은 기존 박스 경계에서 발생합니다. 둥글게 흐려진 모서리도 여전히 판정 영역입니다.

이 문서는 사용자가 수행할 머티리얼 변경 안내입니다. C++와 HLSL 본문만 작성했으며 에셋 수정, 빌드, 셰이더 컴파일 및 실제 렌더링 검증은 수행하지 않았습니다.

## 1. 기존 경계 머티리얼 설정 변경

캐릭터용/카메라 후처리용이 아닌, `BP_InvisibleEvent → Region Fog Material`에 지정한 **영역 경계용 머티리얼**을 엽니다. 원본 보존이 필요하면 복제해서 작업하고 새 머티리얼을 이벤트 BP에 지정합니다.

| 설정 | 값 |
|---|---|
| Material Domain | Surface |
| Blend Mode | TranslucentGreyTransmittance (비 Substrate 환경에서는 Translucent) |
| Two Sided | 체크 |
| Disable Depth Test | **체크** — Details 검색으로 찾기 |
| Translucency Pass | Before DOF 권장 |
| Shading Model | 선택 가능하면 Unlit |

`Disable Depth Test`는 뒷면 프록시가 벽에 가려져 계산 자체가 사라지는 것을 막습니다. 대신 아래 셰이더가 SceneDepth에서 적분을 중단하므로 불투명 벽 너머 안개를 더하지 않습니다. 이 옵션만 켜고 기존 단순 Opacity를 그대로 쓰면 벽을 뚫고 보일 수 있으므로 **반드시 Custom 노드 연결과 함께** 변경합니다. [Epic 머티리얼 속성 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-material-properties)

기존의 `RegionFogOpacity → Opacity` 직결과 Noise/DepthFade → Opacity 연결은 끊습니다. 색상은 기존 `RegionFogColor`의 **RGB → Emissive Color**를 유지합니다. 기존 프로젝트에서 사용한 `Front Material`이 비어 있고 `Opacity Override`/`Emissive Color`가 활성화된 방식 기준입니다.

## 2. Custom 노드 추가

그래프 빈 곳 우클릭 → **Custom** 검색 → 추가합니다.

Custom 노드를 선택한 뒤 Details에서:

- Description: `Soft Region Fog`
- Output Type: **CMOT Float1**
- Code: 같은 폴더의 **[RegionFogOpacity.hlsl](RegionFogOpacity.hlsl) 전체 내용**을 복사해서 붙여 넣습니다.

이 파일은 Custom 노드의 함수 본문입니다. `#include`로 넣거나 별도 셰이더 파일 경로를 등록할 필요가 없습니다. Inputs 배열에 아래 이름을 정확히 추가하고 노드를 연결합니다. [Epic Custom 노드 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/custom-material-expressions-in-unreal-engine)

### 카메라/장면 입력 — 6개

| Custom Input Name | 연결할 노드 |
|---|---|
| WorldPos | Absolute World Position (기본 absolute 좌표, WPO 사용하지 않음) |
| CameraPos | CameraPositionWS |
| OpaqueDepth | SceneDepth (UV 입력은 비움) |
| ProxyDepth | PixelDepth |
| FaceSign | TwoSidedSign |
| TimeSeconds | Time |

`SceneDepth`와 `PixelDepth`는 서로 다른 노드입니다. `TwoSidedSign`은 앞면/뒷면 중 뒷면에서만 계산해 같은 안개가 두 번 합성되지 않도록 사용합니다.

### 영역/모양 입력 — 7개

아래 **Vector Parameter 6개**와 기존 **Scalar Parameter 1개**를 연결합니다. Parameter Name과 Custom Input Name을 동일하게 입력합니다.

| Custom Input / Parameter Name | 노드 종류 | 미리보기 기본값 | 출력 핀 |
|---|---|---|---|
| RegionCenter | Vector Parameter | (0, 0, 0, 0) | RGB |
| RegionAxisX | Vector Parameter | (1, 0, 0, 0) | RGB |
| RegionAxisY | Vector Parameter | (0, 1, 0, 0) | RGB |
| RegionAxisZ | Vector Parameter | (0, 0, 1, 0) | RGB |
| RegionHalfExtent | Vector Parameter | (500, 500, 200, 0) | RGB |
| RegionFogShape | Vector Parameter | (150, 150, 0.01, 0.5) | **RGBA** |
| RegionFogOpacity | Scalar Parameter | 0.2 | 단일 출력 |

Vector Parameter의 숫자는 노드 Details에서 R/G/B/A를 직접 입력합니다. `RegionFogShape`는 색상값이 아니라 **R=감쇠 폭, G=모서리 반경, B=노이즈 주파수, A=노이즈 강도**를 묶은 데이터이므로 RGBA 전체 출력이 필요합니다.

영역 좌표와 축, 크기는 런타임에 코드가 영역별 MID에 자동 설정합니다. 여러 볼륨을 사용해도 한 볼륨의 좌표가 다른 볼륨에 덮어씌워지지 않습니다. 머티리얼 기본값은 미리보기용이며 실제 튜닝은 이벤트 BP에서 합니다.

최종 연결:

```text
위의 입력 13개 → Custom: Soft Region Fog → Opacity Override (또는 Opacity)
RegionFogColor의 RGB → Emissive Color
```

Custom 출력 뒤에 RegionFogOpacity를 다시 곱하거나 DepthFade를 붙이지 않습니다. 이미 밀도/가장자리/장면 깊이가 계산되어 있습니다. Apply/Save 후 사용자가 셰이더 컴파일 결과를 확인합니다.

## 3. C++ 반영 후 이벤트 BP 튜닝

사용자가 C++를 빌드하고 에디터를 다시 연 다음 `BP_InvisibleEvent → Class Defaults → Region Fog`를 확인합니다.

| 설정 | 시작값 | 조절 효과 |
|---|---|---|
| Region Fog Material | 위에서 수정한 머티리얼 | 기존 슬롯 유지 가능 |
| Region Fog Edge Fade Distance | **150cm** | 크게 할수록 경계에서 더 넓게 서서히 흐려짐 |
| Region Fog Corner Radius | **150cm** | 크게 할수록 표시용 모서리가 둥글게 사라짐 |
| Region Fog Noise Scale | 0.01 | 작을수록 농도 무늬가 커짐 |
| Region Fog Noise Strength | 0.5 | 0이면 균일한 내부 농도, 높이면 농도 변화 증가 |
| Region Fog Opacity | **0.08~0.2로 시작** | 이제 고정 표면 알파가 아닌 미터당 밀도 |

처음에는 Edge Fade=150, Corner Radius=150, Opacity=0.1로 확인합니다. 경계가 아직 강하면 Edge Fade를 200~300cm로 늘립니다. 다만 감쇠 폭과 둥글기는 박스의 가장 짧은 반크기 이하로 제한되므로 작은 볼륨에서는 큰 값들이 같은 결과를 낼 수 있습니다.

볼륨이 크면 안개를 통과하는 길이가 길어져 같은 Opacity에서도 더 짙어집니다. 기존 0.2를 그대로 썼을 때 너무 짙으면 0.05~0.1로 줄입니다. 색상은 기존 `Region Fog Color`에서 조절합니다.

**이전 단순 머티리얼을 그대로 쓰면** 필요한 영역 파라미터/Disable Depth Test가 없으므로 경고 후 경계 표시를 생략합니다. 실제 투명화·시야 제한 판정은 계속 동작합니다.

## 수동 확인과 제한

1. 하늘을 배경으로 구역 모서리를 확인합니다. 박스 면에 고정된 일정한 색 대신 가장자리에서 농도가 0으로 감소해야 합니다.
2. Noise Strength=0으로 먼저 감쇠만 확인하고, 0.5로 올려 흐르는 농도를 확인합니다.
3. 영역 안/밖에서 보기, 진입/이탈, 카메라가 모서리를 통과할 때 갑자기 벽이 튀어나오는지 확인합니다.
4. 불투명 벽이 영역 앞에 있으면 안개가 벽 너머로 비치지 않아야 합니다. 벽이 영역 중간에 있으면 카메라에서 벽까지의 안개만 보여야 합니다.
5. 회전/비균일 스케일된 여러 볼륨, 이동/크기 변경, 중도 참가, 이벤트 종료/재시작을 확인합니다.
6. 개인 시야 제한 후처리와 함께 켜고, 반투명 캐릭터·유물·유리를 실제 플레이에서 확인합니다.

이것은 32회 샘플링한 시각 효과이며 물리적인 빛 산란이나 GPU Niagara 입자 시스템이 아닙니다. 광원 빛줄기/그림자 산란은 구현하지 않습니다. 영역이 크거나 많이 겹치면 픽셀 계산량과 반투명 오버드로우가 증가합니다. 겹친 영역의 농도도 합성되므로 더 짙어질 수 있습니다.

깊이를 쓰지 않는 반투명 물체는 SceneDepth로 적분을 중단할 수 없어 정렬에 따른 차이가 남습니다. 현재 플레이의 원근 카메라 기준이며 직교 카메라/VR/모바일은 검증 대상에 포함하지 않았습니다. 머티리얼 미리보기 구의 모습으로 완료 여부를 판단하지 말고 실제 박스와 플레이 카메라에서 확인해야 합니다.
