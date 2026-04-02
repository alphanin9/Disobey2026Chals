FROM ubuntu:24.04

RUN apt-get update && apt-get install -y socat gcc && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

EXPOSE 1337

RUN groupadd -r ctf && useradd -r -g ctf ctf

COPY ez.c /home/ctf/
COPY flag.txt /home/ctf/
WORKDIR /home/ctf
RUN gcc ez.c -std=c2x -O2 -no-pie -fno-stack-protector -D_FORTIFY_SOURCE=0 -o ez

# No sourceleak :)
RUN rm ez.c

USER ctf

CMD ["socat", "tcp-l:1337,reuseaddr,fork", "EXEC:./ez"]
