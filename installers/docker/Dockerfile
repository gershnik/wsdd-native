FROM alpine:latest

RUN <<EOF
set -ex
apk --no-cache add wget
wget -qO-  https://www.gershnik.com/alpine-repo/gershnik@hotmail.com-6643812b.rsa.pub \
        > /etc/apk/keys/gershnik@hotmail.com-6643812b.rsa.pub
mkdir -p /etc/apk/repositories.d 
echo "https://www.gershnik.com/alpine-repo/main" \
      > /etc/apk/repositories.d/www.gershnik.com.list 
apk del wget
apk update
apk --no-cache add wsdd-native

EOF

COPY run-wsddn ./

CMD [ "./run-wsddn"]
