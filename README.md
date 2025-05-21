# cosmOS

<img src="https://cosmit.es/C.svg" alt="cosmOS Logo" height="130"/>

---

## ✨ Bienvenido a cosmOS ✨

cosmOS es un sistema operativo minimalista y personalizado diseñado específicamente para la placa de desarrollo **Luckfox Pico Pro (RV1106)**. Utilizando **Buildroot** y **Make**, construimos un kernel de Linux adaptado a las capacidades y **26 GPIO+** de esta potente y compacta plataforma.

Nuestro objetivo es proporcionar una base sólida y optimizada para proyectos embebidos, robótica y cualquier aplicación que requiera control directo sobre el hardware en la Luckfox Pico Pro.

---

## 🚀 Empezando

Estas instrucciones te guiarán para configurar el entorno de desarrollo necesario para compilar cosmOS.

### 🐳 Entorno de Compilación con Docker

La forma recomendada de compilar cosmOS es usando un contenedor Docker para asegurar un entorno consistente y aislado.

1.  **Crea un directorio compartido (si no existe):**
    ```bash
    mkdir docker-shared
    ```
2.  **Haz el setup para docker:**
    Habilita el sudoless docker para tener permisos en el contenedor..
    ```bash
    docker run -it --rm -v "$(pwd)":/docker-shared/mnt/cosmOS_rv1106 -w /mnt/ luckfoxtech/luckfox_pico:1.0 bash
    ```
    * `/mnt`: El punto de montaje dentro del contenedor.
    *  `docker exec -it cosmOS_development bash`.

3.  **Ejecuta el script post submodules-init de Git:**
    * `sudo sh setup-script.sh`.

### 📦 Instalación de Paquetes Necesarios

Una vez dentro del contenedor Docker (o en tu entorno de compilación nativo si prefieres, aunque no recomendado inicialmente), necesitas instalar las dependencias de software requeridas por Buildroot y las herramientas de compilación.

```bash
apt-get update
apt-get install -y git ssh make gcc gcc-multilib g++-multilib module-assistant expect g++ gawk texinfo libssl-dev bison flex fakeroot cmake unzip gperf autoconf device-tree-compiler libncurses5-dev pkg-config bc python-is-python3 passwd openssl openssh-server openssh-client vim file cpio rsync
