<script>
	import Card from '$lib/components/Card.svelte';
	import { onMount } from 'svelte';

	let accessToken = $state('');
	let refreshToken = $state('');

	onMount(() => {
		accessToken = window.sessionStorage.getItem('date-now-access-token');
		refreshToken = window.sessionStorage.getItem('date-now-refresh-token');
	});
</script>

<Card>
	<p>token={accessToken}\&refresh_token={refreshToken}</p>
	<a href="datenowcms://test">Go to CMS</a>
</Card>

<style>
	p {
		word-break: break-all;
	}
</style>
