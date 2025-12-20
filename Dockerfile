FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    qt6-base-dev qt6-webengine-dev qt6-webchannel-dev qt6-websockets-dev \
    libqt6sql6-psql libpq-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY . .

RUN rm -rf build && cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j$(nproc)

FROM ubuntu:24.04 AS runner

ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем библиотеки для запуска
RUN apt-get update && apt-get install -y --no-install-recommends \
    # GUI и WebEngine
    libqt6gui6 libqt6widgets6 libqt6network6 \
    libqt6webenginecore6 libqt6webenginewidgets6 \
    libqt6webchannel6 \
    libqt6sql6-psql libpq5 \
    # SSL для почты
    libssl3 ca-certificates \
    # Шрифты для отображения текста в браузере и GUI
    fonts-liberation fonts-dejavu fontconfig fonts-noto-color-emoji \
    # Системные зависимости для X11 и DBus
    dbus-x11 libx11-6 libxext6 libxrender1 libxrandr2 \
    && rm -rf /var/lib/apt/lists/*

# Настраиваем кэш шрифтов
RUN fc-cache -fv

WORKDIR /app

COPY --from=builder /src/build/TransportSecureSystem /app/TransportSecureSystem

RUN useradd -m appuser && \
    mkdir -p /tmp/runtime-appuser && \
    chmod 700 /tmp/runtime-appuser && \
    chown appuser:appuser /tmp/runtime-appuser

ENV QT_QPA_PLATFORM=xcb
ENV XDG_RUNTIME_DIR=/tmp/runtime-appuser
ENV QSG_RHI_BACKEND=software
ENV QT_QUICK_BACKEND=software
ENV LIBGL_ALWAYS_SOFTWARE=1
ENV QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox --disable-gpu --disable-dev-shm-usage"

USER appuser

CMD ["/app/TransportSecureSystem", "--no-sandbox"]