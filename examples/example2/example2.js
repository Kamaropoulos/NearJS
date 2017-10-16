'use strict'

console.log(`NearJS Version: ${near.version}`);
console.log(`NodeJS Version: ${process.version}`);
var result = near.helloWorld();
console.log(`helloWorld() returned ${result}`);
var str = near.getString();
console.log(`getString() returned ${str}`);