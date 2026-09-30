# BIP-39 Obfuscator (C++)

Aplicación de escritorio (Linux) para **ofuscar y revertir** frases semilla
BIP-39 con una clave secreta. Dear ImGui + GLFW + OpenGL3, 100% airgapped,
~3.9k líneas propias. Equivalente en C++ de la app Android de referencia:
mismo esquema y, además, **deriva y muestra las direcciones** de la semilla
resultante.

La transformación es un **XOR de la entropía con una clave derivada del
secreto**, seguido de recalcular el checksum BIP-39. El XOR es involutivo:
ofuscar y revertir son la misma operación, siempre que coincidan secreto,
versión de KDF y salt. No hay botón de "ofuscar" y "revertir": el mismo botón
hace ambas cosas según qué frase Introduzcas.

- Sin red: dependencias vendored, filtro seccomp-BPF que bloquea
  `socket()` AF_INET/AF_INET6, test de CTest que verifica que el binario no
  exporta símbolos de red, y perfil AppArmor opcional (`deny network`).
- Sin escritura automática a disco: ni estado de ventana de ImGui, ni caches de
  shaders, ni core dumps (`RLIMIT_CORE = 0`). No exporta archivos: todo vive en
  memoria y se muestra en pantalla.
- Material sensible en memoria `mlock`'ed y auto-zeroed (RAII en todos los
  caminos), incluidas las copias en pila de las direcciones mostradas.
- Portapapeles con auto-clear a los 30 s (configurable con
  `BIP39_CLIPBOARD_TIMEOUT_MS`).
- Lista de palabras embebida en el binario, con verificación SHA-256 si se
  supplya un `bip39.txt` externo.

## Requisitos

- CMake ≥ 3.20, compilador C++20 (GCC ≥ 10 o clang ≥ 12), pkg-config, make.
- libsodium, libsecp256k1 y Dear ImGui: **ya vendored**, no se descargan.
  Inicializa los submódulos si aún no lo están:
  `git submodule update --init --recursive`.
- libsodium se compila estático vía autotools como `ExternalProject` (la primera
  build tarda algo más).
- GLFW3, X11 y OpenGL (dev headers) del sistema: `pkg-config glfw3 x11`.
- `ulimit -l unlimited` recomendado (los buffers se bloquean en RAM con `mlock`).

## Build y tests

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/bip39_obfuscator        # GUI (puede usarse con xvfb-run)
```

Opcionales:

```sh
cmake -S . -B build -DBIP39_BUILD_TESTS=OFF      # sin suite de tests
xvfb-run -a ./build/bip39_obfuscator              # GUI sin pantalla
```

### Tests incluidos

| Target CTest | Qué cubre |
|---|---|
| `bip39_tests` | Vectores BIP-39 oficiales, wordlist + digest, buffers seguros, direcciones EVM/BTC, derivación |
| `clipboard_x11` | Portapapeles X11 real (se salta solo si no hay display) |
| `no_network_symbols` | `nm -D` sobre el binario: ninguna tabla dinámica con símbolos de red/DNS/shell/`dlopen` |

## Uso

1. Pega la seedphrase en el campo multilínea (12, 15, 18, 21 o 24 palabras).
2. Escribe el secreto (campo de contraseña, no se muestra).
3. Elige el KDF:
   - **v1 · SHA-256 (legado)** — `SHA-256(secreto || contador_BE_4B)` concatenado
     hasta cubrir la entropía, sin salt. Paridad exacta con el script Python
     original. Las seeds ofuscadas con v1 **solo** se revierten con v1.
   - **v2 · scrypt (recomendado, por defecto)** —
     `scrypt(secreto, salt, N=32768, r=8, p=1)` vía
     `crypto_pwhash_scryptsalsa208sha256_ll` de libsodium. El salt de 16 bytes
     es **obligatorio e indispensable** para revertir: la app genera uno al
     arrancar y lo muestra en hexadecimal, con botones **Regenerate Salt** y
     **Copy Salt**. Pega ahí el salt guardado para revertir; sin él la
     transformación con v2 no es invertible.
4. Pulsa **Obfuscate / De-obfuscate** y copia el resultado si lo necesitas.
5. Tras la transformación se derivan y muestran las direcciones de la frase
   *resultante*:
   - EVM: `m/44'/60'/0'/0/0` (checksum EIP-55)
   - BTC: `m/84'/0'/0'/0/0` (SegWit nativo P2WPKH, bech32)

> Los parámetros N/r/p de v2 están fijos a propósito. Cambiarlos rompería la
> reversibilidad de las seeds v2 ya ofuscadas: si hay que cambiarlos, crear una
> v3.

## Estructura

```
src/
  main.cpp            # init GLFW/ImGui + hardening de runtime
  secure_mem/         # buffers mlock'ed, secure_string, secure_buffer
  bip39/              # wordlist (con digest)
  crypto/
    kdf.cpp           # v1 SHA-256 contador / v2 scrypt (libsodium)
    seed_transformer.cpp  # XOR de entropía + recomputación de checksum
    keccak256, ripemd160, base58   # soporte para las direcciones
  bip32/              # derivación HD (master + derive_path + fingerprint)
  address/            # evm.cpp, btc.cpp, bech32, addresses (orquestador)
  entropy/            # estimador de entropía (reutilizado del generador)
  clipboard/          # portapapeles X11 con auto-clear
  security/           # seccomp-BPF (bloquea sockets de red)
  ui/                 # app (estado) + pantalla única
tests/                # suite propia + vectores BIP-39 + test de portapapeles
scripts/              # check_no_network.cmake (CTest) + e2e_gui_test.sh
packaging/            # perfil AppArmor opcional
third_party/          # libsodium, libsecp256k1, imgui (vendored/submódulos)
bip39.txt             # 2048 palabras; se embebe y se verifica por SHA-256
```

El core (`bip39_core` = todo `src/` salvo `ui/`, `clipboard/` y `security/`) es
independiente de la GUI y de OpenGL: lo comparten la app y la suite de tests.

## Modelo de seguridad

- **Ofuscación, no cifrado.** Es una capa reversible con clave, no un cifrado
  autenticado: quien tenga la seed ofuscada y el secreto recupera la seed.
  Sirve para que una seed no quede legible a simple vista, no para proteger
  frente a alguien con acceso a ambos. Para eso está la app
  [Encrypt-C++](../Encrypt-C++/README.md) (Argon2id + XChaCha20-Poly1305).
- **El v2 depende del salt.** Sin el salt de 16 bytes, la transformación con v2
  no es invertible. La app lo muestra de forma explícita para que lo guardes.
- **Airegap**: tres capas independientes — sin símbolos de red en el binario,
  filtro seccomp que rechaza `AF_INET`/`AF_INET6` en el kernel (los sockets
  `AF_UNIX` siguen permitidos para X11/Wayland) y, opcionalmente, AppArmor.
  Si el kernel rechaza el filtro la app **arranca igual** (fail-open) y avisa por
  stderr: pierde el refuerzo, no la función.
- **Confidencialidad en memoria**: buffers `mlock`'ed que se ponen a cero con
  `sodium_memzero` al destruirse; `RLIMIT_CORE=0` evita que un dump core
  escriba la frase a disco. Las copias temporales de las direcciones en la pila
  también se limpian.
- **Wordlist**: la copia embebida se usa siempre que el `bip39.txt` externo no
  pase la comprobación de digest.

## Compatibilidad

Mismo esquema que
[Bip39-obfuscator-apk](../Bip39-obfuscator-apk/README.md) (Kotlin) y que el
export por lotes de [Bip39-Generator-C++](../Bip39-Generator-C++/README.md),
que portan la lógica desde el script Python original. Una seed ofuscada por
cualquiera de las tres se revierte con las otras, siempre que coincidan
secreto, versión de KDF y salt.

## Licencia

Apache-2.0 (ver `LICENSE`). Las dependencias vendored conservan sus propias
licencias: libsodium (ISC), libsecp256k1 (MIT), Dear ImGui (MIT).
