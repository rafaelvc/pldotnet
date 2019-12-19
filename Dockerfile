FROM ubuntu:18.04

ARG DOTNET_VERSION=3.1
ARG PG_VERSION=10
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y \
       gcc \
       make \
       wget \
       git  \
       sudo \
       gnupg

RUN wget -q https://packages.microsoft.com/config/ubuntu/16.04/packages-microsoft-prod.deb -O packages-microsoft-prod.deb \
    && dpkg -i packages-microsoft-prod.deb \
    && apt-get update -qq \
    && apt-get install -y dotnet-sdk-"$DOTNET_VERSION" 

RUN wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | apt-key add - \
    && sh -c "echo deb http://apt.postgresql.org/pub/repos/apt/ bionic-pgdg main $PG_VERSION \ 
              >> /etc/apt/sources.list.d/postgresql.list" \
    && apt-get update -qq \
    && apt-get -y -o Dpkg::Options::=--force-confdef -o Dpkg::Options::="--force-confnew" install \ 
       postgresql-"$PG_VERSION" \
       postgresql-server-dev-"$PG_VERSION" \
    && chmod 777 /etc/postgresql/"$PG_VERSION"/main/pg_hba.conf \
    && echo "local   all         postgres                          trust" >  /etc/postgresql/"$PG_VERSION"/main/pg_hba.conf \
    && echo "local   all         all                               trust" >> /etc/postgresql/"$PG_VERSION"/main/pg_hba.conf \
    && echo "host    all         all         127.0.0.1/32          trust" >> /etc/postgresql/"$PG_VERSION"/main/pg_hba.conf \
    && echo "host    all         all         ::1/128               trust" >> /etc/postgresql/"$PG_VERSION"/main/pg_hba.conf \
    && /etc/init.d/postgresql restart \
    && createuser -U postgres -s root 

WORKDIR /home/app

