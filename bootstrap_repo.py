import os
from pathlib import Path

REPO_STRUCTURE = [
    ".github/ISSUE_TEMPLATE",
    ".github/workflows",
    "docs/architecture",
    "docs/hardware",
    "docs/software",
    "docs/ui",
    "docs/api",
    "docs/plugins",
    "docs/milestones",
    "docs/devlog",
    "docs/decisions",
    "firmware/pico",
    "firmware/esp32",
    "firmware/shared",
    "hardware/enclosure",
    "hardware/pcb",
    "hardware/wiring",
    "hardware/cad",
    "hardware/bom",
    "software/audio-engine",
    "software/ui",
    "software/plugins",
    "software/visualizer",
    "software/services",
    "software/drivers",
    "software/tests",
    "sdk",
    "examples",
    "assets",
    "scripts",
    "tools"
]

FILES = {
    "README.md": "",
    ".gitignore": """build/
dist/
*.o
*.bin
*.elf
*.log
.vscode/
.idea/
""",
    ".github/CODEOWNERS": "* @chimera-core",
    ".github/pull_request_template.md": """## Summary

## Changes

## Testing

## Checklist
- [ ] Builds
- [ ] Tests pass
- [ ] No regressions
""",
}

def create_structure():
    for folder in REPO_STRUCTURE:
        Path(folder).mkdir(parents=True, exist_ok=True)

    for file_path, content in FILES.items():
        p = Path(file_path)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content)

    print("Chimera repo scaffold created.")

if __name__ == "__main__":
    create_structure()
