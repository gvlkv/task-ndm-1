FROM	alpine

RUN	apk add --no-cache \
	build-base
RUN	apk add --no-cache \
	meson
RUN	apk add --no-cache \
	linux-headers
WORKDIR	/task-ndm-1
COPY	.	.
RUN	meson setup build \
	&& cd build \
	&& meson compile
