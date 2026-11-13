# 음성 구역과 매몰 효과 연결

대상은 UE **5.8.4**의 기존 엔진 VoIP다. 음성 규칙은 [시스템 설계](../SYSTEM_DESIGN.md)와 [결정 목록](../planning/DECISIONS.md)의 S28·PQ-13을 따른다. 지원 기능은 [온라인·음성 카탈로그](features/ONLINE_VOICE.md), 실제 검증 범위는 [구현 현황](../design/IMPLEMENTATION_STATUS.md)을 확인한다.

## 갱도·던전 구역

던전 영역에는 기존 `ADigetherZoneVolume`을 배치하고 `Zone`을 `Dungeon`으로 지정한다. `ZoneId`는 기존 몬스터 파워 장부 규칙에 따라 비우지 않는다. 공유 맵을 수정하기 전에는 맵 담당·편집 예약·SVN 잠금을 확인한다.

서버는 플레이어 Pawn의 현재 위치로 볼륨을 조회한다. 던전 볼륨 안은 던전, 그 밖은 갱도로 분류하며 겹친 갱도·던전 볼륨은 기존 조회 함수의 던전 우선 규칙을 따른다. 음성은 갱도와 던전 두 종류만 구분하고 던전별 `ZoneId`로 추가 차단하지 않는다. 같은 구역에서는 기존 거리 감쇠만 적용하며 벽이나 층 차폐를 새로 넣지 않는다.

기존 서버 수신 제한 갱신 간격은 0.25초다. 관전자는 관전 대상이 있는 구역의 생존자 목소리를 듣는다. 구역 차단은 음량·매몰 효과보다 먼저 적용하며, 결과 크레딧의 전원 음성에서는 해제한다.

## 매몰 상태 원본

정식 플레이어 Pawn이 기존 Core의 `IDigetherMonsterTargetInterface::IsBuriedForMonster()`를 서버에서 올바르게 반환해야 한다. 음성 코드는 이 조회 결과만 사용하며 매몰 단계·피해·구조를 직접 확정하지 않는다. 인터페이스가 없거나 유효한 Pawn이 없으면 매몰 아님으로 처리한다.

PlayerState의 음성용 매몰 값은 살아 있고 관전 중이 아닌 Pawn에서 재계산하는 복제용 투영값이다. 별도 매몰 게임 상태나 저장 데이터가 아니며 클라이언트 setter/RPC로 변경하지 않는다. 사망·탈출·귀환과 Pawn 교체 때 현재 상태를 다시 읽고, 저장·월드 이동 인계에 이 값을 영구 기록하지 않는다.

2026-11-11 구현 착수 시점에는 정식 Game Pawn에 이 매몰 인터페이스 구현자가 없다. 따라서 음성 처리의 시험 Pawn 검증과 플레이어·굴착 담당의 실제 매몰 사건 통합은 서로 다른 완료 조건이다.

## 거리·먹먹함 조정

- 정상 발화자는 기존 원음 3m·최대 25m를 유지한다.
- 매몰 발화자는 원음 3m를 유지하고 최대 거리를 12.5m로 줄인다. 해당 발화자의 목소리에만 저역통과 필터를 적용한다.
- 관전자끼리와 결과 크레딧의 전원 음성은 거리 감쇠·매몰 필터를 적용하지 않는다.
- 먹먹함의 차단 주파수는 `Project Settings > Game > DIGETHER Options > Voice`의 `Buried Voice Low Pass Hz`로 조정한다. 초기 1500Hz는 청취 조정 전 **시험값**이다. 게임 규칙의 확정 수치나 플레이어 개인 설정이 아니다.

매몰자와 정상 발화자가 함께 말할 때 정상 발화자까지 먹먹해지면 안 된다. 발화 중 매몰 진입·해제와 발화/오디오 재시작 뒤에도 현재 프로필을 사용해야 한다. 기존 전체 목소리 음량·개인별 음량·나만 음소거를 함께 확인한다.

## 리슨 호스트 음소거 해제

UE5.8.4의 리슨 호스트는 서버와 로컬 클라이언트가 같은 PlayerController의 MuteList를 사용한다. 엔진의 게임 음소거 뒤 클라이언트 콜백이 일반 `Muted`까지 같은 명단에 추가하면, 게임 음소거 해제 뒤에도 그 표시가 남는다. 프로젝트의 기존 `ClientMutePlayer` 콜백은 권위·로컬 PC에서 이 중복 기록만 생략하고 실제 음성 인터페이스 호출은 유지한다. 원격 클라이언트는 엔진 기본 경로를 사용한다.

이 보완에서 사용자 음소거·차단 사유를 일괄 삭제하면 안 된다. 게임 규칙에 따른 차단과 사용자 `Muted`·`Blocked`가 겹쳤을 때 게임 차단만 해제되는지 함께 확인한다.

## 엔진 기능과 검증 경계

엔진 `UVOIPTalker::OnTalkingBegin`에서 받은 발화자별 `UAudioComponent`에 감쇠와 LPF를 적용한다. `Settings` 변경만으로 진행 중인 발화가 갱신되지는 않으므로 현재 AudioComponent와 다음 재생 상태를 함께 바꾼다. 엔진의 `USynthComponent` 재시작이 감쇠 값을 다시 복사하므로 소유 Synth 설정도 같은 프로필로 유지한다. 새 음성 제공자·효과 에셋·전역 SoundMix는 필요하지 않다.

이 지원 경로는 로컬 엔진의 `Engine/Public/Net/VoiceConfig.h`, `Engine/Private/Components/AudioComponent.cpp`, `AudioMixer/Private/Components/SynthComponent.cpp`, `OnlineSubsystemUtils/Private/VoiceEngineImpl.cpp`에서 확인했다. 공식 API 참고: [UVOIPTalker](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UVOIPTalker), [AdjustAttenuation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UAudioComponent/AdjustAttenuation), [SetLowPassFilterFrequency](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UAudioComponent/SetLowPassFilterFrequency).

로컬 상태·규칙·오디오 프로퍼티 검사는 실제 마이크 송수신이나 청취 결과를 대신하지 않는다. 실제 음성은 서로 다른 PC의 Steam 세션(M3)에서 정상/매몰 동시 발화, 3m·12.5m·25m 경계, 발화 중 상태 변경, 관전 대상·구역 변경, 크레딧 전환을 확인한다. 같은 프로세스의 다중 PIE는 엔진 VOIPTalker 전역 ID 맵 간섭이 있어 별도 프로세스 검증과 구분한다.

로컬 다인 검사는 [음성 환경 검증 실행기](../../scripts/validation/Invoke-VoiceEnvironmentValidation.ps1)를 사용한다. 에디터 빌드 후 기존 사용자 에디터를 보존하고 검사별 UserDir·설정 사본을 분리한다. 실행기는 시험 Pawn/볼륨을 주입해 4프로세스 상태 복제와 서버 음소거를 검사하며 실제 마이크를 재생하지 않는다.

```powershell
.\scripts\validation\Invoke-VoiceEnvironmentValidation.ps1 -EngineRoot '<UE 5.8.4 설치 폴더>' -Players 4 -OutputRoot '<공백 없는 짧은 검사 폴더>'
```
