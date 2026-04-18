# Security Policy

## Reporting a vulnerability

Please do **not** open a public GitHub issue for security vulnerabilities.

Email **rbutler@tsss.org** with:
- A description of the vulnerability
- Steps to reproduce
- Potential impact

You can expect an acknowledgement within a few days and a fix or mitigation plan within a reasonable timeframe depending on severity.

## Scope

NF0T-Logger is a desktop application that stores credentials (LoTW, eQSL, QRZ, ClubLog) in the system keychain. It communicates with:
- Local radio hardware over serial/USB or LAN (Hamlib, TCI)
- Local digital mode software over UDP (WSJT-X)
- External QSL services over HTTPS (LoTW, eQSL, QRZ, ClubLog)

Issues in any of these trust boundaries are in scope.
