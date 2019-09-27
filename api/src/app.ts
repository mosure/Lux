import { exec, ExecException } from 'child_process';

import express from 'express';
import SmeeClient = require('smee-client');

const app = express();
const port = 3000;

// TODO: Have this return the stdout/stderr log (clip to XXX lines) from exec
app.get('/', (request, response) => {
    response.send('Online!');
});

// Push webhook
app.post('/push', (request, response) => {
    console.log('Push received...');
    exec('../src/git-refresh.sh', (error: ExecException, stdout: string, stderr: string) => {
        console.log(stdout);
        console.log(stderr);
    });

    response.status(200).send();
});

app.listen(port, (err) => {
    if (err) {
        return console.error(err);
    }
    return console.log(`server is listening on ${port}`);
});

// Start a smee.io forwarder for GitHub webhooks
const smee = new SmeeClient({
    logger: console,
    source: 'https://smee.io/RFydymJ4LceIVeNu',
    target: 'http://localhost:' + port + '/push',
});

smee.start();
