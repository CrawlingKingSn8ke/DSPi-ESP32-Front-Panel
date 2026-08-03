# dr_libs decoder sources

This directory vendors the single-header WAV, FLAC and MP3 decoders from
[`mackron/dr_libs`](https://github.com/mackron/dr_libs) at commit
`34a89ffe6bfc4d78db6888fef76cd408dba18185` (2026-07-22).

Included files:

- `dr_wav.h`
- `dr_flac.h`
- `dr_mp3.h`
- `LICENSE`

The DSPi media proof compiles each decoder without its stdio helpers and feeds
it through the project's SdFat-backed `MediaFsFile` callbacks. The upstream project offers a choice
of public-domain or MIT-0 terms; the unmodified upstream license text is
included here.
