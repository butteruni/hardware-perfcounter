# Adreno source provenance

`sources.json` pins the Mesa commit, original paths and SHA256 of the XML/JSON
inputs. Event definitions and BR/BV table associations come from Mesa's
Freedreno registers database. Its MIT permission notice and authors are in
`freedreno_copyright.xml`. `fd7_perfcntr.c` and `fd8_perfcntr.c` at the same Mesa
revision were consulted for their use of these generated tables.

`kgsl_groups.json` records the separately pinned public Qualcomm KGSL UAPI,
including its hash and URL. Only the group ID constants are extracted. The
QUERY ioctl declaration is added to the existing Linux-syscall-note UAPI header
under `third_party/linux`.

Mesa array order and register addresses are not KGSL group IDs. Mesa's slot
counts are not used as runtime capacity: query the vendor kernel. XML event
names and successful GET do not establish event semantics on every product.
The family tables are static candidates, not per-product allowlists.

No proprietary profiler binaries, recovered vendor formulas or application
captures are included in this fork.
