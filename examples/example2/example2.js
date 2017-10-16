'use strict'

console.log(`NearJS Version: ${near.version}`);
console.log(`NodeJS Version: ${process.version}`);

// This method returns an integer
var result = near.helloWorld();
console.log(`helloWorld() returned ${result}`);

// This method returns a string
var str = near.getString();
console.log(`getString() returned ${str}`);

// This should throw an error
near.badMethod();