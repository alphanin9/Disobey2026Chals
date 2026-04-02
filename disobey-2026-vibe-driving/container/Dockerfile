FROM ubuntu:24.04

RUN apt-get update && apt-get install -y socat qemu-system && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN groupadd -r ctf && useradd -r -g ctf ctf

COPY bzImage /home/ctf/
COPY rootfs.cpio.gz /home/ctf/
COPY run.sh /home/ctf/
WORKDIR /home/ctf
USER ctf

EXPOSE 1337
CMD ["socat", "tcp-l:1337,reuseaddr,fork", "EXEC:./run.sh"]
