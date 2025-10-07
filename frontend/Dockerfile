# syntax=docker/dockerfile:1.5

FROM node:20-bullseye-slim AS build

WORKDIR /app

COPY frontend/package*.json frontend/
RUN npm ci --prefix frontend

COPY frontend frontend
COPY backend backend
COPY scripts scripts
COPY CMakeLists.txt ./

RUN ./scripts/build_wasm.sh

WORKDIR /app/frontend
RUN npm run build

FROM nginx:1.27-alpine

COPY --from=build /app/frontend/dist /usr/share/nginx/html
COPY --from=build /app/build/wasm/kolibri.wasm /usr/share/nginx/html/kolibri.wasm

EXPOSE 80

CMD ["nginx", "-g", "daemon off;"]
