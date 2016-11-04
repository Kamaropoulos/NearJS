'ust strict'
const toby = process.binding('toby');

console.log(toby.callback());

setTimeout(function(){
  var ret = toby.callback(function(x){
    console.log(`${x} in callback`);
    return x;
  });
  console.log(ret); // x
},500);


// assgined from example.cpp
console.log(`__val = ${__val}`);

var num = 42;
var foo = 'foo';
global.bar = function(x) {
  return `${foo} bar ${x}`;
}

var result = toby.hostCall('dory', {num, foo});
console.log(`toby.hostCall() = ${result}`);


//setInterval(function(){},1000); // dummy event

return; // exit the scope. atExitCB