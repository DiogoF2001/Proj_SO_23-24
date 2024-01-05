#!/bin/bash

server="/tmp/register_pipe";

for x in {a,b,c,d,test}; do
	./client /tmp/$x.req /tmp/$x.res $server jobs/$x.jobs
done

echo "Done."
