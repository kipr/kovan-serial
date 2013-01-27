#!/bin/sh

rm -Rf include
rm -Rf src

generate()
{
	qdbusxml2cpp -c ${1} -p ${1} -N ${2}.xml
}

echo "Generating Serial class..."
generate Serial org.kipr.Serial

mkdir -p src
mkdir -p include

echo "Moving Headers..."
mv *.h include

echo "Moving Sources..."
mv *.cpp src

echo "Done!"