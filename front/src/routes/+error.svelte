<script>
	import { page } from '$app/state';
	import Button from '$lib/components/Button.svelte';
	import Card from '$lib/components/Card.svelte';
</script>

<Card centered>
	<h1>{page.status} {page.error?.message}</h1>

	<p>Sorry for that 😣</p>

	<Button tag="a" href="/">Go back home</Button>
</Card>
