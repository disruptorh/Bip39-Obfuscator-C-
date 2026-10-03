# BIP-39 Obfuscator (C++)

Aplicación de escritorio (Linux) para **ofuscar y revertir** frases semilla
BIP-39 con una clave secreta. Dear ImGui + GLFW + OpenGL3, 100% airgapped,
~3.9k líneas propias. Equivalente en C++ de la app Android de referencia: mismo
esquema y, además, **deriva y muestra las direcciones** de la semilla
resultante.

La transformación es un **XOR de la entropía con una clave derivada del
secreto**, seguido de recalcular el checksum BIP-39. El XOR es involutivo:
ofuscar y revertir son la misma operación, siempre que coincidan secreto,
versión de KDF y salt. No hay botón de "ofuscar" y "revertir": el mismo botón
hace ambas cosas según qué frase introduzcas.

<p align="center">
  <a href="https://github.com/disruptorh/Bip39-Obfuscator-C-/releases/latest/download/bip39_obfuscator">
    <img alt="Descargar" src="https://img.shields.io/badge/%E2%AC%87%20Download-latest%20release-2f6feb?style=for-the-badge&logo=github&logoColor=white">
  </a>
  <a href="https://github.com/disruptorh/Bip39-Obfuscator-C-/releases/latest">
    <img alt="Versiones" src="https://img.shields.io/github/v/release/disruptorh/Bip39-Obfuscator-C-?label=release&style=flat&logo=github&logoColor=white">
  </a>
  <a href="./LICENSE">
    <img alt="Licencia" src="https://img.shields.io/badge/licencia-Apache--2.0-blue?style=flat">
  </a>
</p>

## 📥 Descarga rápida

El botón de arriba descarga el asset `bip39_obfuscator` de la release más reciente
publicada: un ejecutable **Linux x86-64 sin extensión de fichero**. Es un único
binario, no un instalador ni un `.tar.gz`.

Para usarlo desde una terminal, o para fijarte en una versión concreta:

```bash
curl -L -o bip39_obfuscator https://github.com/disruptorh/Bip39-Obfuscator-C-/releases/latest/download/bip39_obfuscator && chmod +x bip39_obfuscator && ./bip39_obfuscator
```

No es un binario estático: enlaza dinámicamente contra `libOpenGL.so.0`,
`libglfw.so.3` y `libX11.so.6`. En Debian/Ubuntu se resuelven con:

```bash
sudo apt update && sudo apt install -y libopengl-dev libglfw3-dev libx11-dev libgl1
```

Para ejecutarlo sin pantalla (CI, contenedores, SSH sin X11):

```bash
sudo apt install -y xvfb && xvfb-run -a ./bip39_obfuscator
```

## 🚀 Uso rápido

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

El portapapeles se auto-limpia a los 30 s (configurable con la variable de
entorno `BIP39_CLIPBOARD_TIMEOUT_MS`, en milisegundos).

> Los parámetros N/r/p de v2 están fijos a propósito. Cambiarlos rompería la
> reversibilidad de las seeds v2 ya ofuscadas: si hay que cambiarlos, crear una
> v3.

## 📦 Compilar desde código

### Requisitos

- CMake ≥ 3.20, compilador C++20 (GCC ≥ 10 o clang ≥ 12), pkg-config, make.
- Sistema: development headers de GLFW3, X11 y OpenGL.

### Clonar

Este repo **no tiene submódulos**: las tres dependencias están versionadas dentro
del propio repositorio, así que un `git clone` normal basta y no hay nada que
inicializar.

```bash
# 1. Clonar el repositorio
git clone https://github.com/disruptorh/Bip39-Obfuscator-C-.git
cd Bip39-Obfuscator-C--
```

### Dependencias

**Ya vendored en el repo, no hay que instalarlas** (el build no descarga nada de
la red):

| Dependencia | Dónde vive | Nota |
|---|---|---|
| Dear ImGui | `third_party/imgui` | versionado en el repo, parcheado para el perfil airgapped |
| libsodium 1.0.22 | `third_party/libsodium` | versionado en el repo; se compila estático vía autotools (`ExternalProject`) |
| libsecp256k1 | `third_party/libsecp256k1` | versionado en el repo; compilación mínima sin módulos opcionales |

**Hay que instalarlas del sistema** (son las que pide el `CMakeLists.txt` vía
`find_package(OpenGL)` y `pkg_check_modules(glfw3, x11)`):

| Paquete Debian/Ubuntu | Para qué lo pide CMake |
|---|---|
| `build-essential` | g++, make |
| `cmake` | el propio build |
| `pkg-config` | `pkg_check_modules` |
| `libglfw3-dev` | `glfw3` (ventana, contexto GL, portapapeles) |
| `libx11-dev` | `x11` (portapapeles y display) |
| `libopengl-dev` | `find_package(OpenGL)` → `OpenGL::GL` |
| `libgl-dev` | cabeceras y `libGL.so` de Mesa |

```bash
# 2. Instalar las dependencias de compilación (Debian/Ubuntu)
sudo apt update && sudo apt install -y build-essential cmake pkg-config libglfw3-dev libx11-dev libopengl-dev libgl-dev
```

### Compilar

```bash
# 3. Configurar y compilar
cmake -S . -B build && cmake --build build -j
```

El ejecutable queda **directamente en `build/`**: `./build/bip39_obfuscator`.
Nada de `build/Release/`.

La primera build tarda bastante más que las siguientes: libsodium se compila con
autotools y libsecp256k1 también, como `ExternalProject` y `add_subdirectory`
respectivamente.

### Ejecutar los tests

```bash
# 4. Suite de tests (requiere el binario ya compilado)
ctest --test-dir build --output-on-failure
```

| Target CTest | Qué cubre |
|---|---|
| `bip39_tests` | Vectores BIP-39 oficiales, wordlist + digest SHA-256, mezclador de entropía, buffers seguros, derivación y direcciones EVM/BTC |
| `clipboard_x11` | Portapapeles X11 real (se salta solo si no hay display) |
| `no_network_symbols` | `nm -D` sobre el binario: ninguna tabla dinámica con símbolos de red/DNS/shell/`dlopen` |

Los tres los registra `BIP39_BUILD_TESTS` (por defecto `ON`).

### Ejecutar la aplicación

```bash
# 5. Lanzar la GUI
./build/bip39_obfuscator
```

La app usa `mlock` para fijar el material sensible en RAM. Si el límite de
memoria bloqueada es bajo, los buffers no se pueden fijar: el ruido viene de
`ulimit -l`. Se recomienda devolverlo a `unlimited` en tu shell antes de lanzar
la app:

```bash
# 6. Recomendado: sin límite de memoria bloqueada
ulimit -l unlimited && ./build/bip39_obfuscator
```

## 🧰 Comandos útiles / Opciones

Opciones de CMake:

| Opción | Por defecto | Qué hace |
|---|---|---|
| `BIP39_BUILD_TESTS` | `ON` | Compila `bip39_tests`, `bip39_clipboard_test` y registra los tests de CTest |
| `CMAKE_BUILD_TYPE` | `Release` | Se fuerza a `Release` si no lo pasas |

```bash
# Build sin suite de tests (más rápido)
cmake -S . -B build -DBIP39_BUILD_TESTS=OFF && cmake --build build -j
```

Variables de entorno que la app lee:

| Variable | Por defecto | Efecto |
|---|---|---|
| `BIP39_CLIPBOARD_TIMEOUT_MS` | `30000` | Milisegundos hasta el auto-clear del portapapeles |

### Test end-to-end de la GUI

`scripts/e2e_gui_test.sh` conduce la app con teclado sobre un display Xvfb
aislado y verifica el ciclo completo de ofuscación:

1. escribe una mnemónica de 12 palabras y una contraseña;
2. comprueba que el resultado es **otra mnemónica válida del mismo largo** (el
   XOR preserva la longitud de entropía y el checksum se recalcula);
3. "Copy Result" → el portapapeles contiene esa mnemónica ofuscada;
4. "Copiar" (Ethereum) → una dirección `0x…` con checksum EIP-55;
5. "Copiar" (Bitcoin) → una dirección bech32 `bc1q…`;
6. **ida y vuelta**: vuelve a lanzar la app, mete la mnemónica ofuscada con la
   misma contraseña y comprueba que recupera la original (la transformación es un
   XOR con una clave derivada solo de contraseña y salt, luego es una involución);
7. auto-clear del portapapeles tras `BIP39_CLIPBOARD_TIMEOUT_MS`;
8. que la app no escribe nada bajo `$HOME`.

```bash
# 1. Instalar las herramientas del e2e de GUI (Debian/Ubuntu)
sudo apt update && sudo apt install -y xvfb xdotool xclip
```

```bash
# 2. Ejecutar el e2e de la GUI
./scripts/e2e_gui_test.sh
```

La navegación por teclado se **descubre**, no se cuenta a mano: los widgets de
resultado y de direcciones solo existen cuando la ofuscación ya ha corrido, así
que su posición en el orden de tabulación cambia con el layout. El script avanza
widget a widget y se detiene en el primero cuya firma de portapapeles reconoce
(mnemónica / `0x…` / `bc1q…`), que sigue siendo válido si algún día se añade un
widget al formulario.

Dos detalles del guion que conviene conocer:

- Los logs van a un `mktemp -d` propio y se borran al terminar. Poner
  `KEEP_LOGS=1` delante del comando para conservarlos y ver la ruta.
- Xvfb arranca con un `$HOME` propio, separado del de la app: Xvfb lleva GLX y
  corre el rasterizador software de Mesa, y es **ese** proceso quien escribe
  `$HOME/.cache/mesa_shader_cache`. Compartir el `HOME` haría fallar la
  comprobación de "no se persiste nada" por culpa del servidor de display y
  taparía justo lo que debe detectar.

## 🗂️ Estructura del proyecto

```text
.
├── CMakeLists.txt          # targets, vendored, guardas de CTest
├── bip39.txt               # 2048 palabras; se embeben y se verifican por SHA-256
├── packaging/
│   └── bip39_obfuscator.apparmor   # perfil AppArmor (ver nota en Seguridad)
├── scripts/
│   ├── check_no_network.cmake     # guardia de CTest: nm -D sobre el binario
│   └── e2e_gui_test.sh            # e2e de GUI con Xvfb + xdotool + xclip
├── src/
│   ├── main.cpp            # init GLFW/ImGui + hardening de runtime
│   ├── secure_mem/         # buffers mlock'ed, secure_string, secure_buffer
│   ├── bip39/              # wordlist (con digest)
│   ├── crypto/
│   │   ├── kdf.cpp               # v1 SHA-256 contador / v2 scrypt (libsodium)
│   │   ├── seed_transformer.cpp  # XOR de entropía + recomputación de checksum
│   │   └── keccak256, ripemd160, base58   # soporte para las direcciones
│   ├── bip32/              # derivación HD (master + derive_path + fingerprint)
│   ├── address/            # evm.cpp, btc.cpp, bech32, addresses (orquestador)
│   ├── entropy/            # estimador de entropía (reutilizado del generador)
│   ├── clipboard/          # portapapeles X11 con auto-clear
│   ├── security/           # seccomp-BPF (bloquea sockets de red)
│   └── ui/                 # app (estado) + pantalla única
├── tests/                  # suite propia + vectores BIP-39 + test de portapapeles
└── third_party/            # imgui, libsodium y libsecp256k1 (in-tree)
```

El core (`bip39_core` = todo `src/` salvo `ui/`, `clipboard/` y `security/`) es
independiente de la GUI y de OpenGL: lo comparten la app y la suite de tests.

## 🔐 Seguridad

### Modelo de seguridad

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
- **Sin escritura a disco**: ni estado de ventana de ImGui, ni caches de shaders,
  ni core dumps. La app no exporta archivos: todo vive en memoria y se muestra en
  pantalla.

### Compatibilidad

Mismo esquema que
[Bip39-obfuscator-apk](../Bip39-obfuscator-apk/README.md) (Kotlin) y que el
export por lotes de [Bip39-Generator-C++](../Bip39-Generator-C++/README.md),
que portan la lógica desde el script Python original. Una seed ofuscada por
cualquiera de las tres se revierte con las otras, siempre que coincidan
secreto, versión de KDF y salt.

### Activar el perfil AppArmor (opcional)

El perfil está en `packaging/bip39_obfuscator.apparmor` y deniega toda la red
(`deny network`), además de restringir el acceso a ficheros. Declara el perfil
`bip39_obfuscator` sobre `/usr/local/bin/bip39_obfuscator`, así que el binario
tiene que instalarse con ese nombre para que el perfil enganche.

```bash
# Instalar el binario con el nombre que espera el perfil y activar AppArmor
sudo install -m 755 build/bip39_obfuscator /usr/local/bin/bip39_obfuscator && sudo install -m 644 packaging/bip39_obfuscator.apparmor /etc/apparmor.d/bip39_obfuscator && sudo apparmor_parser -r /etc/apparmor.d/bip39_obfuscator
```

Para quitarlo:

```bash
# Quitar el perfil
sudo apparmor_parser -R /etc/apparmor.d/bip39_obfuscator && sudo rm -f /etc/apparmor.d/bip39_obfuscator
```

### Endurecimiento de compilación

El `CMakeLists.txt` aplica a todos los targets: `-Wall -Wextra -Wpedantic
-fstack-protector-strong`, `_FORTIFY_SOURCE=2` y las opciones de enlace
`-pie -Wl,-z,relro,-z,now -Wl,-z,noexecstack`. Dear ImGui se compila con
`IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS` (elimina el "open in shell" con
`fork`/`execvp`) y su loader de OpenGL resuelve los entry points con
`glfwGetProcAddress()` en vez de `dlopen()`.

## 📄 Licencia

Apache-2.0 (ver `LICENSE`). Las dependencias vendored conservan sus propias
licencias: libsodium 1.0.22 (ISC), libsecp256k1 (MIT), Dear ImGui (MIT).