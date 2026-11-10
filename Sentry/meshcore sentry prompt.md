# Project Kickoff: RO-mesh Sentry

You are an autonomous engineering orchestrator deploying a multi-agent system powered by the **Agency Agents** framework (https://github.com/msitarzewski/agency-agents).

You will initialize and scaffold a new custom downstream firmware project named **RO-mesh Sentry**.

---

### Step 1: Persona Evaluation & Local Installation
Inspect the available roles in the `msitarzewski/agency-agents` ecosystem. Evaluate and select the optimal roster of personas required for an embedded C++ firmware engineering workflow:
1. **Identify Roles:** Select personas matching:
   - **Lead Embedded / Systems Architect** (Firmware, C/C++, HAL, SX1262 LoRa, FreeRTOS/PlatformIO).
   - **Security / Threat Modeling Specialist** (Anti-replay analysis, token burn logic, flash state security).
   - **Git Operations & Upstream Maintainer** (Multi-tier fork synchronization, rebase strategies, zero-collision isolation).
2. **Local Installation:**
   - Install the chosen persona definition files locally into `.agency/agents/` (or the project's designated agent config directory).
   - Ensure role responsibilities and interaction protocols are clearly established.

---

### Step 2: Skill Assessment & Provisioning
Evaluate the toolings and skills needed by these agents:
1. Check available agency skills.
2. If any specialized skills are missing, either fetch them or write them directly into the local environment:
   - **`skill-git-fork-sync`**: Commands and heuristics for tri-directional sync (`upstream-dev` -> `fork-jhuebert` -> `ro-mesh-sentry`).
   - **`skill-nvs-token-validator`**: Unit test / mock verification generator for non-volatile storage token burning and NVS serialization.
   - **`skill-lora-packet-parser`**: Steganographic payload extractor for APRS/telemetry and routine check-in strings.

---

### Step 3: Upstream Architecture & Git Topology
We are building a downstream fork with strict isolation to ensure frictionless updates from upstream repositories:
- **Upstream Core:** `https://github.com/meshcore-dev/MeshCore`
- **Upstream Intermediate Fork:** `https://github.com/jhuebert/MeshCore`
- **Our Working Downstream Repo:** `RO-mesh Sentry`

**Rule of Zero Interference:**
- Custom Sentry features must **never** modify upstream core mechanics destructively.
- Encapsulate all logic in an isolated module (e.g., `src/sentry/`) guarded by preprocessor directives (`#ifdef RO_MESH_SENTRY`).
- Intercept incoming traffic using minimal, non-invasive hooks in the packet filtering pipeline.

---

### Step 4: Technical Specification (RO-mesh Sentry)
Implement the functionality defined in Issue #3 (Remote Radio Preset Switching via Filtered Channel Triggers with Single-Use Replay Protection):

#### 1. Radio Presets (NVS Stored)
Support 3 configurable radio profiles stored in non-volatile storage:
- Arguments: `freq` (MHz), `bw` (kHz), `sf` (7–12), `cr` (5 for 4/5).
- Defaults:
  - Preset 1: Base channel (`869.525 62.5 8 5`)
  - Preset 2: Alternate / High-Penetration Offset (`869.650 62.5 9 5`)
  - Preset 3: Fast / Clear Sub-band Offset (`869.400 62.5 7 5`)

#### 2. Channel Monitoring & Inconspicuous Parsing
- Inspect packets received on a designated channel (configurable, e.g., `#public` or `#admin-ops`).
- Extract trigger parameters from natural, low-profile message formats:
  - **Telemetry format:** `WX: Temp <val>, Hum <val>, Stn# <preset_id>-<token>`
  - **Check-in format:** `chk <preset_id> <token> [arbitrary chatter]`

#### 3. Replay-Proof Single-Use Tokens ("Burn-After-Reading")
- Maintain a table of up to 10 disposable tokens in NVS (e.g., `FALCON84`, `RIVER31`).
- Upon matching an active token with a requested preset index:
  1. Immediately mark the token as burned/spent in flash memory.
  2. Persist the requested target preset index as active.
  3. (Optional) Broadcast an inconspicuous telemetry status beacon.
  4. Initiate a 3–5 second hardware reboot to apply the new radio settings.
- Any attempt to reuse a spent token must be silently discarded.

#### 4. CLI Commands
Expose non-conflicting CLI extensions:
- `set sentry.channel <channel_name>`
- `set sentry.tokens <t1> <t2> <t3> ...`
- `set preset <id> <freq> <bw> <sf> <cr>`
- `sentry status`

---

### Execution Deliverables
1. **Agent Setup Manifest:** List of installed personas and any newly written skill modules.
2. **Git Synchronization Guide:** Concrete `git remote` and rebasing playbook for downstream maintenance.
3. **Module Codebase (`src/sentry/`):**
   - `SentryManager.h` & `SentryManager.cpp`: NVS token storage, token burn engine, string parser, and SX1262 preset applicator.
   - Minimal patch / hook injection points into MeshCore's packet filter pipeline.
4. **Validation Test Cases:** Mock unit tests proving single-use token invalidation and rejection of replayed messages.