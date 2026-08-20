<script lang="ts">
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
	import Spinner from '$lib/components/Spinner.svelte';
	import { onMount } from 'svelte';

	const CMS_LOGIN_URL = 'datenowcms://cms.lauradurieux.dev/login';

	let accessToken = $state<string | null>(null);
	let refreshToken = $state<string | null>(null);
	let loaded = $state(false);

	const cmsLink = $derived(
		accessToken && refreshToken
			? `${CMS_LOGIN_URL}?token=${encodeURIComponent(accessToken)}&refresh_token=${encodeURIComponent(refreshToken)}`
			: null
	);

	onMount(() => {
		accessToken = window.sessionStorage.getItem('date-now-access-token');
		refreshToken = window.sessionStorage.getItem('date-now-refresh-token');
		loaded = true;
	});
</script>

<Card centered>
	{#if !loaded}
		<Spinner text="Loading your session..." />
	{:else if cmsLink}
		<h1>You are logged in</h1>

		<p>Open the CMS app to finish signing in. Your session only lives in this browser tab.</p>

		<Button tag="a" href={cmsLink} variant="primary">Go to CMS</Button>

		<details class="profile__tokens">
			<summary>Show tokens</summary>
			<p>token={accessToken}</p>
			<p>refresh_token={refreshToken}</p>
		</details>
	{:else}
		<h1>No active session</h1>

		<p>Your session expired or you are not logged in yet. Ask for a new login link by email.</p>

		<Button tag="a" href="/">Go back home</Button>
	{/if}
</Card>

<style lang="scss">
	.profile {
		&__tokens {
			margin-top: 20px;
			max-width: 100%;

			summary {
				cursor: pointer;
				font-size: 0.875rem;
			}

			p {
				word-break: break-all;
				font-size: 0.875rem;
			}
		}
	}
</style>
