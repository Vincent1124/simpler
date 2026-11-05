==================================================================
onboard-arch-precheck: ARCH MISMATCH — refusing to run.

  Requested platform : --platform a5   (onboard)
  Detected silicon   : a2a3  (SoC=Ascend910_9382, Short_SoC_version=Ascend910_93)

Running onboard tests built for the wrong silicon produces error
cascades that LOOK LIKE genuine bugs but aren't. Typical signatures:

  - CANN 507018 (ACL_ERROR_RT_AICPU_EXCEPTION) at aclrtSynchronizeStreamWithTimeout
  - CANN 507899 at rtStreamCreate / rtMalloc

Both codes can ALSO be genuine bugs — which is precisely what makes
the misdiagnosis costly. CI is fine (matched runners); only local
invocations hit this. See .claude/skills/onboard-arch-precheck/SKILL.md.

Options:
  - Run on this silicon:    --platform a2a3
  - Run silicon-agnostic:   --platform a5sim   (host CPU)
  - Use a different box     (one with --platform a5 silicon)
==================================================================

Connection closed.

Disconnected from remote host(120.9.10.37) at 11:26:01.
