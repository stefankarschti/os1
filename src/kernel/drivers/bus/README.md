# Kernel Bus Drivers

This directory is reserved for bus/device-model code once PCI enumeration grows beyond platform discovery. Today, PCI ECAM walking lives under `platform/` because there is only one real PCI driver family.

Expected future ownership includes driver registration, PCI resource ownership, and common bus binding policy.
