#pragma once

// ADR-0007 section 2: every header under a module's Public/Server/ folder includes this file
// first. StealthPresentation defines STEALTH_PRESENTATION_MODULE=1 privately, so a presentation
// translation unit that reaches a server-only header fails to compile here. The message text is
// matched by the compile probe tools/ci/compile-probes/ServerHeaderInPresentation.cpp: keep it.
#if defined(STEALTH_PRESENTATION_MODULE) && STEALTH_PRESENTATION_MODULE
#error "Server-only header included from StealthPresentation (ADR-0007, IVP R10)"
#endif
