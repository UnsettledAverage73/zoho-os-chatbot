# Zoho OS Docusaurus Docs

This folder contains a standalone Docusaurus site for documenting the Zoho OS kernel without interfering with the main kernel source tree.

## Current structure

- `docs/`: documentation pages
- `src/components/BootstrapStepFrame/`: reusable visualizer embed component
- `static/visualizers/bootstrap_animation.html`: embedded bootstrap visualizer

## Start locally

```bash
cd docs-site
npm install
npm run start
```

## What is already documented

- Boot sequence overview
- GRUB handoff
- Multiboot2 header
- Entry stack setup
- PAE enable
- Page tables and `CR3`
- `EFER.LME`
- Paging enable
- 64-bit GDT and far jump
- 64-bit entry and `kmain()`
- Common bootstrap failures

## Notes

The visualizer supports `?step=N`, which is how each MDX page opens the same animation at the matching stage.
