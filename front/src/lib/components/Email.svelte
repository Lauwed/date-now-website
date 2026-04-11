<script lang="ts">
	import { onMount } from 'svelte';

	let email = $state('');

	onMount(() => {
		email = 'contact@lauradurieux.dev';
	});
</script>

<a href={`mailto:${email}`} class="email">
	contact<span>.example</span>@lauradurieux<span>.com</span>.dev
</a>

<style lang="scss">
	.email {
		span {
			display: none;
		}
	}
</style>
