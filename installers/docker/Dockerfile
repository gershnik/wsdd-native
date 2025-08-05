FROM alpine:latest

RUN apk --no-cache add wget

RUN wget -qO-  https://www.gershnik.com/alpine-repo/gershnik@hotmail.com-6643812b.rsa.pub \
        > /etc/apk/keys/gershnik@hotmail.com-6643812b.rsa.pub

RUN mkdir -p /etc/apk/repositories.d && \
    echo "https://www.gershnik.com/alpine-repo/main" \
      > /etc/apk/repositories.d/www.gershnik.com.list 

RUN apk del wget

RUN apk update

RUN apk --no-cache add wsdd-native

COPY run-wsddn ./

CMD [ "./run-wsddn"]
