# Ledger Stellar App

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Compilation & tests](https://github.com/overcat/ledger-stellar/actions/workflows/ci-workflow.yml/badge.svg)](https://github.com/overcat/ledger-stellar/actions/workflows/ci-workflow.yml)

## Introduction

This is the wallet app for the [Ledger Nano S](https://shop.ledger.com/products/ledger-nano-s), [Ledger Nano S Plus](https://shop.ledger.com/pages/ledger-nano-s-plus) and [Ledger Nano X](https://shop.ledger.com/pages/ledger-nano-x) that makes it possible to store [Stellar](https://www.stellar.org/)-based assets on those devices and generally sign any transaction for the Stellar network.

## Documentation

This app follows the specification available in the [docs](./docs/) folder.

## SDK

You can communicate with the app through the following SDKs:

- [JavaScript library](https://github.com/LedgerHQ/ledger-live/blob/develop/libs/ledgerjs/packages/hw-app-str/README.md)
- [Python library](https://github.com/overcat/strledger)

## Building and installing

If not for development purposes, you should install this app via [Ledger Live](https://www.ledger.com/ledger-live).

To build and install the app on your Nano S, Nano S Plus or Nano X you must set up the Ledger build environments. Please follow the *load the application instructions* at the [Ledger developer portal](https://developers.ledger.com/docs/nano-app/load/).

Additionaly, install this dependency:

```shell
sudo apt install libbsd-dev
```

The command to compile and load the app onto the device is:

```shell
make load
```

To remove the app from the device do:

```shell
make delete
```