exec_id=$(curl localhost:2375/v1.40/containers/bash/exec -d '{"User":"","Privileged":false,"Tty":true,"AttachStdin":true,"AttachStderr":true,"AttachStdout":true,"Detach":false,"DetachKeys":"ctrl-e,e","Env":null,"WorkingDir":"","Cmd":["bash"]}' -H 'Content-Type: application/json' | jq -r '.Id')

(cat <<!
POST /v1.40/exec/$exec_id/start HTTP/1.1
Host: localhost
User-Agent: Docker-Client/19.03.2 (darwin)
Content-Length: 28
Connection: Upgrade
Content-Type: application/json
Upgrade: tcp

{"Detach":false,"Tty":true}
!
) | tee >(pbcopy)
nc localhost 2375
exit 0

export tty=/dev/ttys007
stty -f $tty -echo
stty -f $tty -icanon
stty -f $tty -isig
stty -f $tty -icrnl
stty -f $tty -inlcr
stty -f $tty -igncr
stty -f $tty -opost
stty -f $tty min 1
stty -f $tty time 0
