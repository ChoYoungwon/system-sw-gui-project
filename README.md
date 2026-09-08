> **POSIX 시스템 콜과 System V IPC(Message Queue) 기반의 비동기 GUI 파일 관리자 및 샌드박스 커스텀 셸**  
> 프론트엔드(UI)와 백엔드(시스템 데몬)의 프로세스 분리 아키텍처를 설계하고, 시스템 보안(Directory Traversal 방어) 및 논블로킹(Non-blocking) IPC 동기화를 구현한 리눅스 시스템 소프트웨어 프로젝트입니다.

---

## 0. 목차 (Table of Contents)

- [0. 목차 (Table of Contents)](#0-목차-table-of-contents)
- [1. 프로젝트 개요 (Overview)](#1-프로젝트-개요-overview)
- [2. 시스템 아키텍처 \& 핵심 설계 (System Architecture)](#2-시스템-아키텍처--핵심-설계-system-architecture)
  - [2.1 프로세스 분리 및 IPC 통신 모델](#21-프로세스-분리-및-ipc-통신-모델)
  - [2.2 System V Message Queue 프로토콜 정의](#22-system-v-message-queue-프로토콜-정의)
  - [2.3 논블로킹(Non-blocking) UI 비동기 동기화](#23-논블로킹non-blocking-ui-비동기-동기화)
  - [2.4 Custom Shell 샌드박스 보안 설계](#24-custom-shell-샌드박스-보안-설계)
- [3. 주요 모듈 및 기술 상세 (Key Features)](#3-주요-모듈-및-기술-상세-key-features)
  - [모듈 1: GTK4 File Manager (`dir_manage_gtk`)](#모듈-1-gtk4-file-manager-dir_manage_gtk)
  - [모듈 2: Sandboxed Custom Shell (`custom_shell`)](#모듈-2-sandboxed-custom-shell-custom_shell)
  - [모듈 3: IPC 분석 \& Docker 멀티 컨테이너 소켓 (`IPC`, `virtual_container`)](#모듈-3-ipc-분석--docker-멀티-컨테이너-소켓-ipc-virtual_container)
- [4. 기술적 난제 및 문제 해결 (Engineering Challenges \& Troubleshooting)](#4-기술적-난제-및-문제-해결-engineering-challenges--troubleshooting)
  - [Q1. GTK4 단일 이벤트 루프에서 백엔드 IPC 대기 시 UI 프리징 현상](#q1-gtk4-단일-이벤트-루프에서-백엔드-ipc-대기-시-ui-프리징-현상)
  - [Q2. 비어있지 않은 대용량 디렉토리 삭제 시 `rmdir` 실패 처리](#q2-비어있지-않은-대용량-디렉토리-삭제-시-rmdir-실패-처리)
  - [Q3. 상대 경로 입력에 따른 샌드박스 경로 탈출(Directory Traversal) 취약점](#q3-상대-경로-입력에-따른-샌드박스-경로-탈출directory-traversal-취약점)
  - [Q4. 크로스 디바이스(Cross-Device) 및 대용량 파일 이동 시 `rename()` 실패 Fallback](#q4-크로스-디바이스cross-device-및-대용량-파일-이동-시-rename-실패-fallback)
- [5. 기술 스택 \& 개발 환경 (Tech Stack)](#5-기술-스택--개발-환경-tech-stack)
- [6. 프로젝트 디렉토리 구조 (Project Structure)](#6-프로젝트-디렉토리-구조-project-structure)
- [7. 빌드 및 실행 가이드 (Getting Started)](#7-빌드-및-실행-가이드-getting-started)
  - [7.1 필수 패키지 설치 (Ubuntu/Debian 기준)](#71-필수-패키지-설치-ubuntudebian-기준)
  - [7.2 GTK File Manager 빌드 및 실행](#72-gtk-file-manager-빌드-및-실행)
  - [7.3 Custom Shell 빌드 및 실행](#73-custom-shell-빌드-및-실행)
  - [7.4 Virtual Container (Docker 소켓 통신) 실행](#74-virtual-container-docker-소켓-통신-실행)

---

## 1. 프로젝트 개요 (Overview)

![GTK File Manager UI](https://github.com/user-attachments/assets/c91ab84e-d4b3-4e00-8e20-67db94615e4c)

* **개발 기간**: 2024.11 ~ 2024.12
* **핵심 목표**:
  * 리눅스 OS의 핵심 기능(프로세스, 파일 시스템, 시그널, IPC)을 이해하고 C 언어로 직접 구현
  * 단일 프로세스 구조의 한계를 극복하기 위해 **UI 프로세스(GTK4)** 와 **파일 제어 백엔드**를 분리한 분산형 아키텍처 적용
  * 셸(Shell) 구현 시 경로 조작 공격(Path Traversal)을 방어하는 **보안 샌드박스(Sandbox Jail)** 구축

---

## 2. 시스템 아키텍처 & 핵심 설계 (System Architecture)

### 2.1 프로세스 분리 및 IPC 통신 모델

GTK UI 메인 프로세스가 파일 I/O나 무거운 시스템 작업을 직접 수행할 경우 UI 프리징(멈춤)이 발생할 수 있습니다. 이를 방지하기 위해 **프론트엔드와 백엔드를 독립된 프로세스로 분리**하고 **System V Message Queue**로 통신하도록 설계했습니다.
1. **프로세스 라이프사이클 관리**: UI 프로그램 실행 시 `fork()`와 `execl("./_build/a.out")`을 호출하여 백엔드 데몬을 자식 프로세스로 자동 구동합니다. UI 종료 시(`on_application_shutdown`) 백엔드에 종료 신호(Type 1: `QUIT`)를 전달하여 고아 프로세스 생성을 방지합니다.
2. **IPC 메시지 큐 분리**: 명령 전달용(`command_keyfile`)과 결과 응답용(`response_keyfile`) 큐를 물리적으로 분리하여 메시지 혼선을 방지했습니다.

---

### 2.2 System V Message Queue 프로토콜 정의

메시지 큐 버퍼 구조체(`struct mymsgbuf`)의 `mtype`을 기반으로 요청 명령을 라우팅합니다.

| `mtype` | 메시지 성격 | 페이로드 포맷 (`mtext`) | 백엔드 처리 동작 (POSIX API) |
|:---:|:---:|:---|:---|
| **1** | System Control | `"QUIT"` | 백엔드 프로세스 안전 종료 |
| **2** | Delete | `target_path` | 파일(`unlink`) 또는 디렉토리 재귀 삭제(`nftw`) |
| **3** | Mkdir | `dir_path` | 디렉토리 생성(`mkdir`, 중복 시 자동 넘버링 접미사 처리) |
| **4** | Rename | `old_name$new_name` | 파일/폴더 이름 변경(`rename`, `strtok_r`) |
| **5** | Move | `src_path$dest_path` | 이동 (`rename` 우선 시도 $\rightarrow$ 실패 시 `opendir`/`readdir` 복사 후 원본 삭제) |
| **6** | Copy | `src_path$dest_path` | 파일(`fread`/`fwrite` 버퍼 스트림) 및 디렉토리 재귀 복사 + 권한 보존(`chmod`) |
| **7** | Chmod | `path$octal_mode` | 파일/폴더 권한 변경 (`strtol(..., 8)` $\rightarrow$ `chmod`) |
| **8** | Favorite (Symlink) | `src_path$dest_path` | `/tmp/link` 내 심볼릭 링크 생성/갱신 (`symlink`, `unlink`) |
| **10** | **Backend Response** | `"success"` / `"failed"` | UI로 작업 결과 통보 및 비동기 화면 갱신 트리거 |

---

### 2.3 논블로킹(Non-blocking) UI 비동기 동기화

단일 스레드 기반의 GTK4 이벤트 루프 환경에서 백엔드 응답을 동기식(`Blocking`)으로 대기하면 사용자 인터페이스가 멈추게 됩니다.

```c
// communication.c: 비차단 방식의 응답 폴링
gboolean read_backend_message(gpointer user_data) {
    UserData *data = (UserData *)user_data;
    struct mymsgbuf mbuf;
    ...
    // IPC_NOWAIT 플래그를 통해 큐에 메시지가 없어도 즉시 반환 (UI 프리징 방지)
    if (msgrcv(response_msgid, &mbuf, sizeof(mbuf.mtext), 10, IPC_NOWAIT) != -1) {
        on_backend_message_received(mbuf.mtext, data);
    }
    return TRUE; // GLib Timer 유지
}
```
* `g_timeout_add(1000, read_backend_message, data)`를 통해 1초 주기로 백엔드 응답을 비차단 폴링합니다.
* 응답(`"success"`) 수신 시 `g_timeout_add`로 렌더링 타이밍을 분산(`update` +200ms, `update_favorites_list` +300ms)하여 대량의 파일 갱신 시에도 부드러운 UI 반응성을 확보했습니다.

---

### 2.4 Custom Shell 샌드박스 보안 설계

커스텀 셸(`custom_shell`)은 사용자가 임의의 루트 디렉토리를 훼손하지 못하도록 `/tmp/test` 영역을 논리적 루트(`/`)로 취급하는 **샌드박스(Jail)** 환경을 갖추고 있습니다.

* **경로 정규화 (`convert_to_absolute`)**: `.`(현재 디렉토리) 및 `..`(상위 디렉토리) 토큰을 스택 방식으로 계산하여 실제 도달할 정규화된 절대 경로를 사전에 계산합니다.
* **샌드박스 탈출 검증 (`is_valid_path`)**: 정규화된 경로가 `BASE_PATH`(`/tmp/test`) 접두사를 벗어나는 경우 시스템 콜 호출을 즉시 차단합니다.
* **명령어 디스패치 테이블 (`cmd_t`)**: 함수 포인터 배열 구조를 도입하여 O(1) 수준의 빠른 디스패칭과 컴파일 타임 매크로 분기(`ENABLE_CMD_*`)를 지원합니다.

---

## 3. 주요 모듈 및 기술 상세 (Key Features)

### 모듈 1: GTK4 File Manager (`dir_manage_gtk`)

| 구분 | 구현 기능 | 주요 기술 및 시스템 콜 |
|:---|:---|:---|
| **파일 탐색 & 뷰** | • 다중 컬럼 파일 브라우징 (아이콘, 이름, 크기, 수정시간)<br>• 주소 표시줄 직접 이동 및 상위 폴더 이동 | `GtkColumnView`, `GtkDirectoryList`, `GFileInfo`, `GFile` |
| **파일/폴더 관리** | • 우클릭 컨텍스트 메뉴를 통한 직관적 조작<br>• 폴더 생성, 이름 변경, 안전 삭제 | `GtkGestureClick`, `GtkDialog`, `mkdir`, `rename`, `unlink` |
| **재귀적 파일 제어** | • 비어있지 않은 디렉토리의 전체 트리 삭제<br>• 파일 및 디렉토리 복사/이동 (스트림 버퍼링) | `nftw(FTW_DEPTH)`, `opendir`, `readdir`, `fread`/`fwrite`, `chmod` |
| **권한 & 바로가기** | • 파일/디렉토리 8진수 퍼미션 변경<br>• 심볼릭 링크 기반 즐겨찾기(Favorites) 사이드바 | `chmod`, `strtol`, `symlink`, `GtkListBox` |

### 모듈 2: Sandboxed Custom Shell (`custom_shell`)

* **내장 명령어 지원**:
  * **디렉토리 제어**: `cd`, `mkdir`, `rmdir`, `ls` (옵션 `-a`, `-l`, `-al` 포맷팅 완벽 지원)
  * **파일 제어**: `cat`, `cp`, `rm`, `rename`, `chmod`
  * **링크 관리**: `ln` (하드링크 `link` 및 심볼릭 링크 `symlink -s`)
  * **프로세스 관리**: `ps` (`/proc` 파싱을 통한 PID/상태 출력), `kill` (`kill` 시스템 콜)
  * **외부 프로그램 실행**: `run` (`fork`, `execv`/`execvp`, `waitpid`)
* **시그널 제어**: `signal(SIGINT, SIG_IGN)` 설정을 통해 Ctrl+C 입력 시 셸 프로세스가 비정상 종료되는 것을 방지

### 모듈 3: IPC 분석 & Docker 멀티 컨테이너 소켓 (`IPC`, `virtual_container`)

* **SysV IPC 5종 메커니즘 비교 구현 (`IPC/`)**:
  1. `01_pipe.c`: 익명 단방향 파이프 (`pipe`, `fork`)
  2. `02_named_pipe.c`: FIFO 파일 기반 양방향 통신 (`mkfifo`, `open`, `read`, `write`)
  3. `03_shared_memory.c`: 공유 메모리 세그먼트 할당 및 참조 (`shmget`, `shmat`, `shmdt`, `shmctl`)
  4. `04_semaphore.c`: 임계 구역 동기화 및 락 제어 (`semget`, `semop`, `semctl`)
  5. `05_message_queue.c`: 구조화된 비동기 메시징 (`msgget`, `msgsnd`, `msgrcv`)
* **Docker Compose 기반 TCP 소켓 통신 (`virtual_container/`)**:
  * 격리된 브릿지 네트워크 환경에서 `socket-server`와 `socket-client` 간 `AF_INET` TCP 스트림 소켓 통신 환경 구성

---

## 4. 기술적 난제 및 문제 해결 (Engineering Challenges & Troubleshooting)

### Q1. GTK4 단일 이벤트 루프에서 백엔드 IPC 대기 시 UI 프리징 현상
* **문제 상황**: 백엔드에서 대용량 파일 복사나 재귀 삭제를 수행하는 동안 `msgrcv`를 블로킹 모드로 호출하면 GTK UI의 이벤트 루프가 멈춰 창이 응답 없음 상태에 빠짐.
* **해결 방안**:
  1. `msgrcv` 호출 시 `IPC_NOWAIT` 플래그를 설정하여 메시지가 없으면 에러(`ENOMSG`)를 반환하고 즉시 제어권을 반환하도록 구성.
  2. GLib 메인 컨텍스트에 타이머 소스(`g_timeout_add(1000, ...)`)를 등록하여 백그라운드에서 주기적으로 비차단 폴링 수행.
  3. UI 렌더링 갱신 시 `g_timeout_add`로 200ms의 완충 시간을 두어 파일 시스템 동기화 완료 후 뷰를 안전하게 리프레시.

---

### Q2. 비어있지 않은 대용량 디렉토리 삭제 시 `rmdir` 실패 처리
* **문제 상황**: `rmdir()` 시스템 콜은 비어있는 디렉토리만 삭제 가능하므로, 내부에 파일이나 하위 디렉토리가 존재하는 폴더 삭제 시 `ENOTEMPTY` 오류 발생.
* **해결 방안**:
  * POSIX 파일 트리 순회 함수인 `nftw()`(New File Tree Walk) 도입.
  * 플래그로 `FTW_DEPTH`(하위 항목을 먼저 방문하는 후위 순회)와 `FTW_PHYS`(심볼릭 링크 대상 추적 방지)를 지정하여 콜백 함수(`remove_callback`)에서 최하위 파일부터 안전하게 `remove()`/`unlink()`를 호출하도록 구현.

```c
// backend.c
int remove_file_or_directory(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) return -1;

    if (S_ISDIR(path_stat.st_mode)) {
        if (rmdir(path) == 0) return 0; // 빈 디렉토리 바로 삭제
        // 비어있지 않은 경우 nftw 후위 순회로 하위 요소부터 재귀 삭제
        return nftw(path, remove_callback, 64, FTW_DEPTH | FTW_PHYS);
    } else {
        return unlink(path); // 일반 파일
    }
}
```

---

### Q3. 상대 경로 입력에 따른 샌드박스 경로 탈출(Directory Traversal) 취약점
* **문제 상황**: 커스텀 셸에서 사용자가 `cd ../../` 또는 `rm -rf /` 등의 경로를 입력할 경우 시스템의 실제 루트 디렉토리나 중요 파일이 손상될 위험 존재.
* **해결 방안**:
  * 입력 경로를 토큰 단위(`/`)로 분해하여 `.`과 `..`을 정규화하는 `convert_to_absolute()` 유틸리티 함수 구현.
  * 최종 목적지 문자열이 `BASE_PATH`(`/tmp/test`)로 시작하는지 `strncmp`로 엄격하게 검증하여 탈출 시도를 사전에 원천 차단.

---

### Q4. 크로스 디바이스(Cross-Device) 및 대용량 파일 이동 시 `rename()` 실패 Fallback
* **문제 상황**: `rename()` 시스템 콜은 동일한 파일 시스템(마운트 지점) 내에서만 원자적 이동이 가능하며, 파티션이 다르거나 디렉토리 구조가 복잡할 경우 `EXDEV` 오류를 발생시키며 실패함.
* **해결 방안**:
  * `move_directory()` 함수에서 1차적으로 `rename()`을 시도하고, 실패할 경우 8KB 스트림 버퍼 기반의 `copy_file()` $\rightarrow$ 원본 `remove_file_or_directory()` 2단계 트랜잭션 방식으로 Fallback 처리하여 안정성 보장.

---

## 5. 기술 스택 & 개발 환경 (Tech Stack)

| 분류 | 기술 스택 |
|:---|:---|
| **언어 (Language)** | `C (C99 Standard / POSIX.1-2001)` |
| **GUI 프레임워크** | `GTK 4.0`, `GLib 2.0`, `GIO`, `GResource` |
| **시스템 프로그래밍** | `System V IPC (Message Queue, Shared Memory, Semaphore)`, `POSIX Threads`, `Signals`, `Procfs` |
| **빌드 시스템** | `Meson`, `Ninja`, `GNU Make`, `GCC` |
| **가상화 & 컨테이너** | `Docker`, `Docker Compose` |
| **타깃 환경** | `Linux (Ubuntu 22.04 LTS / Debian-based)` |

---

## 6. 프로젝트 디렉토리 구조 (Project Structure)

```plaintext
system-sw-gui-project/
├── dir_manage_gtk/               # [모듈 1] GTK4 기반 GUI 파일 관리자
│   ├── main.c                    # UI 초기화, 백엔드 fork/exec, 이벤트 액션 바인딩
│   ├── backend.c                 # POSIX 파일 I/O 및 IPC 응답 처리 백엔드 데몬
│   ├── communication.c           # Message Queue 통신 및 논블로킹 UI 폴링
│   ├── header.h                  # 공유 구조체(UserData, mymsgbuf) 및 함수 프로토타입
│   ├── ui_list.c                 # GtkColumnView 컬럼 렌더링 및 메타데이터 팩토리
│   ├── ui_manage.c               # 우클릭 컨텍스트 메뉴 및 CRUD 다이얼로그
│   ├── ui_cd.c                   # 디렉토리 이동, 복사/이동 액션 핸들러
│   ├── ui_link.c                 # 즐겨찾기(Favorites) 심볼릭 링크 관리
│   ├── column.ui                 # GtkBuilder XML UI 템플릿
│   ├── column.gresource.xml      # GTK 리소스 번들 설정
│   └── meson.build               # Meson 빌드 스크립트
│
├── custom_shell/                 # [모듈 2] 샌드박스 커스텀 셸 (CLI)
│   ├── main.c                    # 셸 프롬프트 루프, 시그널 핸들링, 커맨드 디스패처
│   ├── path_utils.c              # 경로 정규화 및 샌드박스 유효성 검증
│   ├── config.h / config.c       # 명령어 활성화 매크로 및 상수 정의
│   ├── custom_header.h           # 공통 헤더
│   ├── cmd_*.c                   # 개별 명령어 구현체 (ls, cd, mkdir, rm, cp, ps, kill, run 등)
│   └── Makefile                  # GNU Make 빌드 스크립트
│
├── IPC/                          # [모듈 3] 리눅스 IPC 5종 학습 & 구현
│   ├── 01_pipe.c                 # Anonymous Pipe
│   ├── 02_named_pipe.c           # Named Pipe (FIFO)
│   ├── 03_shared_memory.c        # Shared Memory
│   ├── 04_semaphore.c            # Semaphore
│   └── 05_message_queue.c        # Message Queue
│
├── virtual_container/            # [모듈 4] Docker 멀티 컨테이너 TCP 통신
│   ├── Dockerfile                # C 개발 환경 컨테이너 빌드
│   ├── docker-compose.yml        # Server-Client 격리 네트워크 정의
│   ├── tcp_server.c              # TCP 스트림 소켓 서버
│   └── tcp_client.c              # TCP 스트림 소켓 클라이언트
│
└── README.md                     # 프로젝트 포트폴리오 문서
```

---

## 7. 빌드 및 실행 가이드 (Getting Started)

### 7.1 필수 패키지 설치 (Ubuntu/Debian 기준)

```bash
sudo apt update
sudo apt install -y build-essential libgtk-4-dev meson ninja-build
```

---

### 7.2 GTK File Manager 빌드 및 실행

```bash
# 1. 디렉토리 이동
cd dir_manage_gtk

# 2. Meson 빌드 설정 (빌드 디렉토리 생성)
meson setup _build

# 3. Ninja 컴파일
ninja -C _build

# 4. GUI 파일 관리자 실행 (백엔드 a.out 프로세스는 자동으로 fork 실행됩니다)
./_build/column
```

> **주의**: 백엔드 프로세스(`a.out`)는 `main.c`의 `start_backend()`에 의해 `_build/a.out` 경로로 실행되므로, `_build` 디렉토리 내에서 실행 파일이 생성되어야 합니다.

---

### 7.3 Custom Shell 빌드 및 실행

```bash
# 1. 디렉토리 이동
cd custom_shell

# 2. 컴파일
make

# 3. 셸 실행
./program
```

```plaintext
# 샌드박스 셸 프롬프트 예시
/ $ mkdir test_dir
/ $ cd test_dir
/test_dir $ ls -al
/test_dir $ cd ../../  (샌드박스 외부 접근 시 자동으로 / 로 제한)
/ $ quit
```

---

### 7.4 Virtual Container (Docker 소켓 통신) 실행

```bash
cd virtual_container

# 컨테이너 빌드 및 실행
docker compose up -d

# 클라이언트 로그 확인
docker compose logs -f client
```

