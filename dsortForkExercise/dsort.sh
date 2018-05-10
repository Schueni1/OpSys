#!/bin/bash
( $1; $2 ) | sort | uniq -d
