'use strict'

// print near.version
console.log(`node :: near.version = ${near.version}`);

// assgined from example1.cpp
console.log(`near :: _v = ${_v}`);

var num = 42;
var foo = 'foo';

near.on('test', function(x){
  console.log(`node :: near.on(test) = ${x}`);
});

var result = near.hostCall('dory', {num, foo});
console.log(`node :: near.hostCall() = ${result}`);

process.on('test', near.hostOn('exit'));
process.on('exit', near.hostOn('exit'));

process.emit('test', 'a', 20, {num, foo});

// exit after 2 secs
(function(){setTimeout(function(){
	process.exitCode = 42;
},2000)})();