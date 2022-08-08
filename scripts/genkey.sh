#!/bin/bash

cat > ssl-ca.conf <<ENDF
[req]
default_bits = 2048
prompt = no
default_md = sha256
distinguished_name = dn
x509_extensions = usr_cert
[ dn ]
DC=com
DC=example
CN=webroot
[ usr_cert ]
basicConstraints=CA:TRUE,pathlen:3
subjectKeyIdentifier=hash
authorityKeyIdentifier=keyid,issuer
keyUsage = keyCertSign, cRLSign
ENDF

cat > ssl-dispatcher.conf <<ENDF
[req]
default_bits = 2048
prompt = no
default_md = sha256
x509_extensions = req_ext
req_extensions = req_ext
distinguished_name = dn
[ dn ]
DC=com
DC=example
CN=localhost
[ req_ext ]
subjectAltName = @alt_names
[ alt_names ]
DNS.1 = localhost
DNS.2 = dispatcher.example.com
ENDF

cat > ssl-client.conf <<ENDF
[req]
default_bits = 2048
prompt = no
default_md = sha256
x509_extensions = req_ext
req_extensions = req_ext
distinguished_name = dn
[ dn ]
DC=com
DC=example
CN=client
[ req_ext ]
subjectAltName = @alt_names
[ alt_names ]
DNS.1 = localhost
DNS.2 = client.example.com
ENDF

function gen_client_key_and_cert()
{
    KEY_NAME=$1.key
    REQ_NAME=$1.req
    CERT_NAME=$1.crt
    PROFILE_NAME=ssl-$1.conf
    openssl genrsa -out ${KEY_NAME} 2048 && \
    openssl req -new -key ${KEY_NAME} -out ${REQ_NAME} -config ${PROFILE_NAME} && \
    openssl x509 -req -in ${REQ_NAME} -CA ca-web.crt -CAkey ca-web.key -CAcreateserial \
        -out ${CERT_NAME} -days 365 -extensions 'req_ext' -extfile ${PROFILE_NAME} && \
    openssl verify -CAfile ca-web.crt ${CERT_NAME} && \
    openssl x509 -in ${CERT_NAME} -noout -text|grep DNS
}

openssl genrsa -out ca-web.key 2048 || exit 1
openssl req -new -x509 -key ca-web.key -days 730 -out ca-web.crt -config ssl-ca.conf || exit 1

gen_client_key_and_cert dispatcher || exit 1
gen_client_key_and_cert client || exit 1

rm *.req *.conf
