# Hercules-TMS570LC43x

Worked examples for the TI **LAUNCHXL2-570LC43** LaunchPad (TMS570LC4357, dual-core
lockstep Cortex-R5F). Each example is a self-contained HALCoGen and Code Composer
Studio project.

New to the board? Start with the [setup guide](docs/setup.md). It covers installing
the tools, generating a project, building, and flashing, with screenshots.

## Layout

```
docs/                 setup guide and images
examples/
  NN-name/
    README.md         what it shows, pins, expected result
    halcogen/         HALCoGen project (.hcg) with generated source/ and include/
    ccs/              Code Composer Studio project that links to halcogen/
```

Application code is written inside the `USER CODE` markers of the HALCoGen files,
so regenerating does not overwrite it.

## Examples

| # | Folder | Topic |
|---|--------|-------|
| 00 | [`00-blink-gio`](examples/00-blink-gio) | GIO outputs and inputs: LED blink and a button |

## TI documentation

- [LAUNCHXL2-570LC43 tool page](https://www.ti.com/tool/LAUNCHXL2-570LC43#tech-docs): board documents, schematic, quick start guide
- [TMS570LC4357 product page](https://www.ti.com/product/TMS570LC4357#tech-docs): datasheet, Technical Reference Manual, application notes

## Tools

HALCoGen 04.07.01, Code Composer Studio 21.0.1, TI Arm Compiler 20.2.7.LTS.
