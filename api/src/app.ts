import { SmeeClient } from 'smee-client';

import express from 'express';

const app = express();
const port = 3000;

app.get('/', (request, response) => {
    response.send('Status endpoint');
});

// Push webhook
app.post('/push', (request, response) => {
    // TODO: Implement config.json write from body and git-refresh.sh
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

const events = smee.start();

// Stop forwarding events
events.close();
