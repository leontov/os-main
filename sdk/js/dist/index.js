import fetch from "node-fetch";
export class KolibriAgent {
    constructor(baseUrl = "http://127.0.0.1:8056") {
        this.baseUrl = baseUrl;
    }
    async step(payload) {
        const response = await fetch(this.prefix("/api/agent/step"), {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload),
        });
        if (!response.ok) {
            throw new Error(`step failed: ${response.status} ${response.statusText}`);
        }
        return (await response.json());
    }
    async state() {
        const response = await fetch(this.prefix("/api/agent/state"));
        if (!response.ok) {
            throw new Error(`state failed: ${response.status} ${response.statusText}`);
        }
        return (await response.json());
    }
    prefix(endpoint) {
        return `${this.baseUrl.replace(/\/$/, "")}${endpoint}`;
    }
}
